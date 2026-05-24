#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "emu.h"
#include "cpu.h"
#include "bus.h"
#include "uart.h"
#include "clint.h"
#include "disk.h"
#include "input.h"
#include "fb.h"

#define BATCH_SIZE      2000000     /* instructions per SDL poll */
#define CLINT_DIVISOR   1           /* mtime ticks per instruction */
#define IRQ_CHECK_INTERVAL 1024     /* check interrupts every N insns */
#define FB_REFRESH_MS   33          /* ~30 fps */

static struct termios orig_termios;
static bool           termios_saved = false;

static void restore_terminal(void)
{
    if (termios_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static void sig_handler(int sig)
{
    (void)sig;
    restore_terminal();
    _exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <kernel.bin> [disk.img]\n", argv[0]);
        return 1;
    }

    const char *kernel_path = argv[1];
    const char *disk_path   = (argc >= 3) ? argv[2] : NULL;

    /* ── Save terminal state ────────────────────────────────────── */
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        termios_saved = true;
    }
    atexit(restore_terminal);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /* ── Allocate RAM ───────────────────────────────────────────── */
    uint8_t *ram = calloc(1, RAM_SIZE);
    if (!ram) {
        fprintf(stderr, "Failed to allocate %d MB RAM\n", RAM_SIZE / (1024*1024));
        return 1;
    }

    /* ── Load kernel binary into RAM ────────────────────────────── */
    FILE *kf = fopen(kernel_path, "rb");
    if (!kf) {
        fprintf(stderr, "Cannot open kernel: %s\n", kernel_path);
        free(ram);
        return 1;
    }
    fseek(kf, 0, SEEK_END);
    long ksz = ftell(kf);
    fseek(kf, 0, SEEK_SET);
    if (ksz > (long)RAM_SIZE) {
        fprintf(stderr, "Kernel too large (%ld bytes)\n", ksz);
        fclose(kf);
        free(ram);
        return 1;
    }
    if ((long)fread(ram, 1, ksz, kf) != ksz) {
        fprintf(stderr, "Failed to read kernel\n");
        fclose(kf);
        free(ram);
        return 1;
    }
    fclose(kf);
    printf("[emu] Loaded kernel: %s (%ld bytes)\n", kernel_path, ksz);

    /* ── Init SDL2 ──────────────────────────────────────────────── */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free(ram);
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow(
        "HerOS RISC-V Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        FB_WIDTH, FB_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(ram);
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    /* ── Init subsystems ────────────────────────────────────────── */
    bus_init(ram, RAM_SIZE);
    uart_init();
    clint_init();
    disk_init(disk_path, ram);
    input_init();
    fb_init(renderer);

    /* ── Init CPU ───────────────────────────────────────────────── */
    cpu_t cpu;
    cpu_init(&cpu, RAM_BASE);

    printf("[emu] Starting execution at 0x%08X\n", RAM_BASE);

    /* ── Main loop ──────────────────────────────────────────────── */
    bool     running   = true;
    uint32_t last_fb   = SDL_GetTicks();
    uint32_t last_fps  = SDL_GetTicks();
    int      frame_count = 0;

    while (running && !cpu.halted) {
        /* Execute a batch of instructions */
        for (int i = 0; i < BATCH_SIZE && !cpu.halted; i++) {
            cpu_step(&cpu);

            /* Check timer & interrupts periodically (not every insn) */
            if ((i & (IRQ_CHECK_INTERVAL - 1)) == 0) {
                clint_tick(IRQ_CHECK_INTERVAL * CLINT_DIVISOR);

                /* Update mip from CLINT state (set AND clear) */
                if (clint_timer_pending())
                    cpu.mip |= MIP_MTIP;
                else
                    cpu.mip &= ~MIP_MTIP;
                if (clint_software_pending())
                    cpu.mip |= MIP_MSIP;
                else
                    cpu.mip &= ~MIP_MSIP;

                /* Check for pending interrupts */
                cpu_check_interrupts(&cpu);
            }
        }

        /* ── Poll SDL events ────────────────────────────────────── */
        SDL_Event ev;
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE &&
                    (ev.key.keysym.mod & KMOD_CTRL)) {
                    running = false;
                } else {
                    input_push_key(ev.key.keysym.scancode,
                                   ev.key.keysym.mod, 1);
                }
                break;
            case SDL_KEYUP:
                input_push_key(ev.key.keysym.scancode,
                               ev.key.keysym.mod, 0);
                break;
            case SDL_TEXTINPUT:
                for (int i = 0; ev.text.text[i]; i++) {
                    if ((uint8_t)ev.text.text[i] < 128)
                        input_push_text(ev.text.text[i]);
                }
                break;
            case SDL_MOUSEMOTION: {
                int mx = ev.motion.x * FB_WIDTH / win_w;
                int my = ev.motion.y * FB_HEIGHT / win_h;
                input_push_mouse_move(mx, my);
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                int mx = ev.button.x * FB_WIDTH / win_w;
                int my = ev.button.y * FB_HEIGHT / win_h;
                input_push_mouse_button(mx, my, ev.button.button, 1, SDL_GetModState());
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                int mx = ev.button.x * FB_WIDTH / win_w;
                int my = ev.button.y * FB_HEIGHT / win_h;
                input_push_mouse_button(mx, my, ev.button.button, 0, SDL_GetModState());
                break;
            }
            case SDL_MOUSEWHEEL: {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                mx = mx * FB_WIDTH / win_w;
                my = my * FB_HEIGHT / win_h;
                input_push_scroll(mx, my, ev.wheel.y);
                break;
            }
            }
        }

        /* ── Refresh framebuffer at ~30fps ──────────────────────── */
        uint32_t now = SDL_GetTicks();
        if (fb_is_dirty() && (now - last_fb >= FB_REFRESH_MS)) {
            fb_refresh();
            last_fb = now;
            frame_count++;
        }

        /* ── Update FPS in window title every second ──────────── */
        if (now - last_fps >= 1000) {
            char title[64];
            snprintf(title, sizeof(title), "HerOS RISC-V Emulator — %d FPS", frame_count);
            SDL_SetWindowTitle(window, title);
            fprintf(stderr, "[emu] %d FPS\n", frame_count);
            frame_count = 0;
            last_fps = now;
        }
    }

    /* ── Cleanup ────────────────────────────────────────────────── */
    printf("\n[emu] Halted after %llu instructions\n",
           (unsigned long long)cpu.insn_count);

    fb_cleanup();
    disk_close();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(ram);

    return 0;
}
