#include "fb.h"
#include "bus.h"
#include <string.h>

static uint8_t       pixels[FB_SIZE];
static SDL_Texture  *texture;
static SDL_Renderer *renderer;
static bool          dirty;

void fb_init(SDL_Renderer *rend)
{
    renderer = rend;
    memset(pixels, 0, FB_SIZE);
    dirty = true;

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ABGR8888,  /* matches RGBA byte order */
                                SDL_TEXTUREACCESS_STREAMING,
                                FB_WIDTH, FB_HEIGHT);
}

uint8_t fb_read(uint32_t addr)
{
    if (addr < FB_SIZE)
        return pixels[addr];
    return 0;
}

void fb_write(uint32_t addr, uint8_t val)
{
    if (addr < FB_SIZE) {
        pixels[addr] = val;
        dirty = true;
    }
}

uint32_t fb_ctrl_read(uint32_t offset)
{
    switch (offset) {
    case FB_CTRL_WIDTH:   return FB_WIDTH;
    case FB_CTRL_HEIGHT:  return FB_HEIGHT;
    default:              return 0;
    }
}

void fb_ctrl_write(uint32_t offset, uint32_t val)
{
    if (offset == FB_CTRL_FLUSH && val == 1) {
        fb_refresh();
    } else if (offset == FB_CTRL_DMA_SRC) {
        /* DMA blit: copy FB_SIZE bytes from guest RAM to pixel buffer */
        if (val >= RAM_BASE && val + FB_SIZE <= RAM_BASE + RAM_SIZE) {
            uint8_t *ram = bus_get_ram();
            memcpy(pixels, ram + (val - RAM_BASE), FB_SIZE);
            dirty = true;
            fb_refresh();
        }
    }
}

void fb_refresh(void)
{
    if (!texture || !renderer) return;

    SDL_UpdateTexture(texture, NULL, pixels, FB_WIDTH * FB_BPP);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    dirty = false;
}

bool fb_is_dirty(void)
{
    return dirty;
}

void fb_cleanup(void)
{
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
}

uint8_t *fb_get_pixels(void)
{
    return pixels;
}

/* ── GPU 2D accelerator ─────────────────────────────────────────── */
static uint32_t gpu_regs[GPU_SIZE / 4];

uint32_t gpu_read(uint32_t offset)
{
    if (offset < GPU_SIZE)
        return gpu_regs[offset / 4];
    return 0;
}

void gpu_write(uint32_t offset, uint32_t val)
{
    if (offset >= GPU_SIZE) return;

    if (offset == GPU_CMD) {
        /* Execute command using previously written register values */
        uint32_t x     = gpu_regs[GPU_X / 4];
        uint32_t y     = gpu_regs[GPU_Y / 4];
        uint32_t w     = gpu_regs[GPU_W / 4];
        uint32_t h     = gpu_regs[GPU_H / 4];
        uint32_t color = gpu_regs[GPU_COLOR / 4];
        uint32_t src   = gpu_regs[GPU_SRC_ADDR / 4];
        uint32_t dst   = gpu_regs[GPU_DST_ADDR / 4];

        uint8_t *ram = bus_get_ram();

        switch (val) {
        case GPU_CMD_FILL_BUF: {
            /* Fill rect in a RAM-based buffer (backbuffer) */
            /* dst = RAM address of buffer start, stride = GPU_STRIDE */
            uint32_t stride = gpu_regs[GPU_STRIDE / 4];
            if (dst < RAM_BASE || !stride) break;
            uint32_t buf_off = dst - RAM_BASE;

            /* Clip to RAM bounds */
            for (uint32_t row = y; row < y + h; row++) {
                uint32_t row_start = buf_off + row * stride + x * 4;
                if (row_start + w * 4 > RAM_SIZE) break;
                uint32_t *p = (uint32_t *)(ram + row_start);
                for (uint32_t col = 0; col < w; col++)
                    p[col] = color;
            }
            break;
        }
        case GPU_CMD_COPY: {
            /* memcpy: src→dst, length = W bytes */
            if (src >= RAM_BASE && dst >= RAM_BASE) {
                uint32_t s_off = src - RAM_BASE;
                uint32_t d_off = dst - RAM_BASE;
                if (s_off + w <= RAM_SIZE && d_off + w <= RAM_SIZE)
                    memcpy(ram + d_off, ram + s_off, w);
            }
            break;
        }
        default:
            break;
        }
    } else {
        gpu_regs[offset / 4] = val;
    }
}
