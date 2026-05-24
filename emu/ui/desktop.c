#include "desktop.h"
#include "types.h"
#include "draw.h"
#include "theme.h"
#include "animation.h"
#include "wm.h"
#include "lockscreen.h"
#include "app_registry.h"
#include "icons.h"
#include "../kernel/kprintf.h"
#include "../kernel/sched.h"
#include "../kernel/string.h"

/* Forward declarations for shell components */
extern void topbar_render(void);
extern void sidebar_left_render(int mx, int my);
extern void sidebar_right_render(void);
extern void dock_render(int mx, int my);
extern int  dock_handle_click(int x, int y);

/* Forward declarations for app factories */
extern AppContent *terminal_create(void);
extern AppContent *filemanager_create(void);
extern AppContent *settings_create(void);
extern AppContent *taskmanager_create(void);

/* ── Geometric background pattern ────────────────────────────── */
static void render_background(void)
{
    const ThemeColors *tc = theme_colors();

    /* Vertical gradient */
    draw_gradient_v(RECT(0, 0, SCREEN_W, SCREEN_H), tc->bg_top, tc->bg_bottom);

    /* Subtle geometric overlay */
    uint32_t t = hal_get_ticks() / 100;

    /* Floating circles */
    for (int i = 0; i < 6; i++) {
        int cx = 200 + i * 180 + hal_sin256((int)(t + i * 40)) * 30 / 256;
        int cy = 200 + hal_cos256((int)(t * 2 + i * 60)) * 50 / 256;
        int r = 40 + i * 10;
        Color c = tc->accent;
        c.a = 8 + i * 2;
        draw_filled_circle_blend(cx, cy, r, c);
    }

    /* Grid lines */
    Color grid = tc->panel_border;
    grid.a = 15;
    for (int x = 0; x < SCREEN_W; x += 80)
        for (int y = 0; y < SCREEN_H; y += 2)
            hal_fb_pixel_blend(x, y, grid);
    for (int y = 0; y < SCREEN_H; y += 80)
        for (int x = 0; x < SCREEN_W; x += 2)
            hal_fb_pixel_blend(x, y, grid);
}

/* ── Desktop main loop ───────────────────────────────────────── */
void desktop_main(void)
{
    kprintf("[desktop] Starting desktop compositor\n");

    /* Init UI subsystems */
    theme_init();
    anim_init();
    wm_init();
    lockscreen_init();
    app_registry_init();

    /* Register apps */
    app_registry_register("Terminal", ICON_TERMINAL, terminal_create);
    app_registry_register("Files", ICON_FILES, filemanager_create);
    app_registry_register("Settings", ICON_SETTINGS, settings_create);
    app_registry_register("Tasks", ICON_TASK_MGR, taskmanager_create);

    kprintf("[desktop] Registered %d apps\n", app_registry_count());
    kprintf("[desktop] Lock screen active (password: heros)\n");

    uint32_t frame_count = 0;
    uint32_t last_fps_time = hal_get_ticks();
    int fps = 0;

    /* ── Main compositor loop ────────────────────────────────── */
    while (1) {
        uint32_t frame_start = hal_get_ticks();
        int mx, my;
        hal_input_mouse_pos(&mx, &my);

        /* ── Poll input events ───────────────────────────────── */
        hal_event_t evt;
        while (hal_input_poll(&evt)) {
            if (lockscreen_is_locked()) {
                /* Lock screen handles input */
                switch (evt.type) {
                case HAL_EVT_KEY_DOWN:
                    lockscreen_handle_key(evt.key, evt.mod);
                    break;
                case HAL_EVT_TEXT:
                    lockscreen_handle_text(evt.ch);
                    break;
                default:
                    break;
                }
                continue;
            }

            /* Route events to desktop/WM */
            switch (evt.type) {
            case HAL_EVT_KEY_DOWN:
                /* Global shortcuts */
                if (evt.key == HAL_KEY_L && (evt.mod & HAL_MOD_CTRL)) {
                    lockscreen_lock();
                } else {
                    wm_handle_key_down(evt.key, evt.mod);
                }
                break;

            case HAL_EVT_TEXT:
                wm_handle_text_input(evt.ch);
                break;

            case HAL_EVT_MOUSE_DOWN: {
                /* Check dock first */
                int dock_app = dock_handle_click(evt.mouse_x, evt.mouse_y);
                if (dock_app >= 0) {
                    app_registry_launch(dock_app);
                } else if (evt.mouse_y < 28) {
                    /* Topbar click — ignore for now */
                } else {
                    wm_handle_mouse_down(evt.mouse_x, evt.mouse_y, evt.mouse_btn);
                }
                break;
            }

            case HAL_EVT_MOUSE_UP:
                wm_handle_mouse_up(evt.mouse_x, evt.mouse_y, evt.mouse_btn);
                break;

            case HAL_EVT_MOUSE_MOVE:
                wm_handle_mouse_move(evt.mouse_x, evt.mouse_y);
                break;

            case HAL_EVT_SCROLL:
                wm_handle_scroll(evt.mouse_x, evt.mouse_y, evt.scroll_y);
                break;

            default:
                break;
            }
        }

        /* ── Tick animations ─────────────────────────────────── */
        wm_tick();

        /* ── Render ──────────────────────────────────────────── */

        /* Background */
        render_background();

        if (lockscreen_is_locked()) {
            lockscreen_render();
        } else {
            /* Sidebars */
            sidebar_left_render(mx, my);
            sidebar_right_render();

            /* Windows */
            wm_render();

            /* Top bar (over windows) */
            topbar_render();

            /* Dock */
            dock_render(mx, my);

            /* Mouse cursor */
            draw_filled_rect(RECT(mx, my, 2, 2), COLOR_WHITE);
            draw_line(mx, my, mx + 8, my + 6, COLOR_WHITE);
            draw_line(mx, my, mx + 3, my + 10, COLOR_WHITE);
        }

        /* ── Flush to display ────────────────────────────────── */
        hal_fb_flush();

        /* ── FPS counter ─────────────────────────────────────── */
        frame_count++;
        if (hal_get_ticks() - last_fps_time >= 5000) {
            fps = (int)(frame_count * 1000 / (hal_get_ticks() - last_fps_time));
            kprintf("[desktop] FPS: %d\n", fps);
            frame_count = 0;
            last_fps_time = hal_get_ticks();
        }

        /* ── Frame rate limiting (~30 fps target) ────────────── */
        uint32_t frame_time = hal_get_ticks() - frame_start;
        if (frame_time < 33)
            sched_sleep_ms(33 - frame_time);
    }
}
