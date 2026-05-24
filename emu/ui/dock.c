#include "types.h"
#include "draw.h"
#include "theme.h"
#include "icons.h"
#include "app_registry.h"

#define DOCK_H        48
#define DOCK_ICON_SZ  32
#define DOCK_PADDING  8

void dock_render(int mx, int my)
{
    const ThemeColors *tc = theme_colors();
    int dock_y = SCREEN_H - DOCK_H;

    /* Background */
    draw_filled_rect_blend(RECT(0, dock_y, SCREEN_W, DOCK_H), tc->dock_bg);

    /* Top border */
    draw_line(0, dock_y, SCREEN_W, dock_y, tc->panel_border);

    /* Dock icons — centered */
    int app_count = app_registry_count();
    int total_w = app_count * (DOCK_ICON_SZ + DOCK_PADDING) - DOCK_PADDING;
    int start_x = (SCREEN_W - total_w) / 2;

    for (int i = 0; i < app_count; i++) {
        const AppManifest *app = app_registry_get(i);
        if (!app) continue;

        int ix = start_x + i * (DOCK_ICON_SZ + DOCK_PADDING);
        int iy = dock_y + (DOCK_H - DOCK_ICON_SZ) / 2;
        Rect icon_rect = RECT(ix - 4, iy - 4,
                               DOCK_ICON_SZ + 8, DOCK_ICON_SZ + 8);
        int hover = rect_contains(icon_rect, mx, my);

        /* Hover highlight */
        if (hover)
            draw_filled_rounded_rect_blend(icon_rect, 8,
                COLOR(255, 255, 255, 30));

        /* Running indicator */
        if (app->running)
            draw_filled_circle(ix + DOCK_ICON_SZ / 2,
                               dock_y + DOCK_H - 6, 2, tc->accent);

        /* Icon */
        Color ic = hover ? tc->accent : tc->text_primary;
        icon_draw(app->icon, ix, iy, DOCK_ICON_SZ, ic);
    }
}

/* Returns app index if dock icon was clicked, -1 otherwise */
int dock_handle_click(int x, int y)
{
    int dock_y = SCREEN_H - DOCK_H;
    if (y < dock_y) return -1;

    int app_count = app_registry_count();
    int total_w = app_count * (DOCK_ICON_SZ + DOCK_PADDING) - DOCK_PADDING;
    int start_x = (SCREEN_W - total_w) / 2;

    for (int i = 0; i < app_count; i++) {
        int ix = start_x + i * (DOCK_ICON_SZ + DOCK_PADDING);
        int iy = dock_y + (DOCK_H - DOCK_ICON_SZ) / 2;
        Rect icon_rect = RECT(ix - 4, iy - 4,
                               DOCK_ICON_SZ + 8, DOCK_ICON_SZ + 8);
        if (rect_contains(icon_rect, x, y))
            return i;
    }
    return -1;
}
