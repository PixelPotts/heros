#include "icons.h"
#include "draw.h"

static void icon_terminal(int x, int y, int s, Color c)
{
    /* Terminal: rectangle with ">_" prompt */
    draw_rounded_rect(RECT(x, y, s, s), s/8, c);
    draw_line(x + s/5, y + s/3, x + s*2/5, y + s/2, c);
    draw_line(x + s/5, y + s*2/3, x + s*2/5, y + s/2, c);
    draw_line(x + s/2, y + s*2/3, x + s*3/4, y + s*2/3, c);
}

static void icon_files(int x, int y, int s, Color c)
{
    /* File manager: folder shape */
    draw_filled_rounded_rect(RECT(x, y + s/5, s, s*4/5), s/10, c);
    draw_filled_rect(RECT(x, y + s/10, s*2/5, s/5), c);
}

static void icon_settings(int x, int y, int s, Color c)
{
    /* Settings: gear (circle with teeth) */
    int cx = x + s/2, cy = y + s/2;
    draw_circle(cx, cy, s/3, c);
    draw_circle(cx, cy, s/6, c);
    for (int i = 0; i < 8; i++) {
        int angle = i * 32;
        int px = cx + hal_cos256(angle) * s * 2 / (5 * 256);
        int py = cy + hal_sin256(angle) * s * 2 / (5 * 256);
        draw_filled_rect(RECT(px-1, py-1, 3, 3), c);
    }
}

static void icon_task_mgr(int x, int y, int s, Color c)
{
    /* Task manager: bar chart */
    int bw = s / 5;
    draw_filled_rect(RECT(x + s/8, y + s*3/5, bw, s*2/5), c);
    draw_filled_rect(RECT(x + s*3/8, y + s*2/5, bw, s*3/5), c);
    draw_filled_rect(RECT(x + s*5/8, y + s/5, bw, s*4/5), c);
}

static void icon_close(int x, int y, int s, Color c)
{
    /* X */
    int m = s / 4;
    draw_line(x + m, y + m, x + s - m, y + s - m, c);
    draw_line(x + s - m, y + m, x + m, y + s - m, c);
}

static void icon_minimize(int x, int y, int s, Color c)
{
    draw_line(x + s/4, y + s*3/4, x + s*3/4, y + s*3/4, c);
}

static void icon_maximize(int x, int y, int s, Color c)
{
    draw_rect(RECT(x + s/4, y + s/4, s/2, s/2), c);
}

static void icon_folder(int x, int y, int s, Color c)
{
    draw_filled_rounded_rect(RECT(x, y + s/4, s, s*3/4), s/10, c);
    draw_filled_rect(RECT(x, y + s/6, s*2/5, s/4), c);
}

static void icon_file(int x, int y, int s, Color c)
{
    draw_rect(RECT(x + s/6, y, s*2/3, s), c);
    draw_line(x + s/6 + s/6, y, x + s*5/6, y + s/6, c);
    /* Text lines */
    draw_line(x + s/4, y + s*2/5, x + s*3/4, y + s*2/5, c);
    draw_line(x + s/4, y + s*3/5, x + s*3/4, y + s*3/5, c);
}

static void icon_home(int x, int y, int s, Color c)
{
    /* House shape */
    draw_line(x + s/2, y + s/6, x + s/8, y + s/2, c);
    draw_line(x + s/2, y + s/6, x + s*7/8, y + s/2, c);
    draw_rect(RECT(x + s/5, y + s/2, s*3/5, s*2/5), c);
}

static void icon_search(int x, int y, int s, Color c)
{
    draw_circle(x + s*2/5, y + s*2/5, s/4, c);
    draw_line(x + s*3/5, y + s*3/5, x + s*4/5, y + s*4/5, c);
}

static void icon_lock(int x, int y, int s, Color c)
{
    /* Lock body */
    draw_filled_rounded_rect(RECT(x + s/5, y + s*2/5, s*3/5, s*3/5), s/10, c);
    /* Shackle */
    int cx = x + s/2;
    for (int angle = 64; angle < 192; angle++) {
        int px = cx + hal_cos256(angle) * s / (5 * 256);
        int py = y + s*2/5 + hal_sin256(angle) * s / (5 * 256);
        hal_fb_pixel_blend(px, py, c);
    }
}

static void icon_power(int x, int y, int s, Color c)
{
    int cx = x + s/2, cy = y + s/2;
    /* Circle */
    for (int angle = 30; angle < 226; angle++) {
        int px = cx + hal_cos256(angle) * s / (3 * 256);
        int py = cy + hal_sin256(angle) * s / (3 * 256);
        hal_fb_pixel_blend(px, py, c);
    }
    /* Vertical line */
    draw_line(cx, y + s/5, cx, y + s/2, c);
}

static void icon_wifi(int x, int y, int s, Color c)
{
    int cx = x + s/2;
    /* Three arcs */
    for (int r = 1; r <= 3; r++) {
        int radius = s * r / 7;
        for (int angle = 96; angle < 160; angle++) {
            int px = cx + hal_cos256(angle) * radius / 256;
            int py = y + s*3/4 + hal_sin256(angle) * radius / 256;
            hal_fb_pixel_blend(px, py, c);
        }
    }
    /* Dot at bottom */
    hal_fb_pixel_blend(cx, y + s*3/4, c);
}

static void icon_battery(int x, int y, int s, Color c)
{
    draw_rect(RECT(x + s/8, y + s/3, s*5/8, s/3), c);
    draw_filled_rect(RECT(x + s*3/4, y + s*5/12, s/8, s/6), c);
    /* Fill level */
    draw_filled_rect(RECT(x + s/8 + 2, y + s/3 + 2, s*3/8, s/3 - 4), c);
}

static void icon_clock(int x, int y, int s, Color c)
{
    int cx = x + s/2, cy = y + s/2;
    draw_circle(cx, cy, s*2/5, c);
    draw_line(cx, cy, cx, cy - s/4, c);
    draw_line(cx, cy, cx + s/5, cy, c);
}

typedef void (*icon_fn_t)(int x, int y, int s, Color c);

static const icon_fn_t icon_fns[ICON_COUNT] = {
    [ICON_TERMINAL]  = icon_terminal,
    [ICON_FILES]     = icon_files,
    [ICON_SETTINGS]  = icon_settings,
    [ICON_TASK_MGR]  = icon_task_mgr,
    [ICON_CLOSE]     = icon_close,
    [ICON_MINIMIZE]  = icon_minimize,
    [ICON_MAXIMIZE]  = icon_maximize,
    [ICON_FOLDER]    = icon_folder,
    [ICON_FILE]      = icon_file,
    [ICON_HOME]      = icon_home,
    [ICON_SEARCH]    = icon_search,
    [ICON_LOCK]      = icon_lock,
    [ICON_POWER]     = icon_power,
    [ICON_WIFI]      = icon_wifi,
    [ICON_BATTERY]   = icon_battery,
    [ICON_CLOCK]     = icon_clock,
};

void icon_draw(IconId id, int x, int y, int size, Color c)
{
    if (id < ICON_COUNT && icon_fns[id])
        icon_fns[id](x, y, size, c);
}
