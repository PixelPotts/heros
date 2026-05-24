#include "types.h"
#include "draw.h"
#include "theme.h"
#include "icons.h"
#include "widgets.h"
#include "../kernel/mm.h"
#include "../kernel/sched.h"
#include "../kernel/string.h"
#include "../kernel/kprintf.h"

#define SIDEBAR_W  48
#define RIGHT_SIDEBAR_W 180

/* Left sidebar: app launcher icons */
static const struct {
    IconId icon;
    const char *tooltip;
} sidebar_items[] = {
    { ICON_HOME,     "Home" },
    { ICON_TERMINAL, "Terminal" },
    { ICON_FILES,    "Files" },
    { ICON_SETTINGS, "Settings" },
    { ICON_TASK_MGR, "Tasks" },
    { ICON_SEARCH,   "Search" },
};
#define SIDEBAR_ITEM_COUNT 6

void sidebar_left_render(int mx, int my)
{
    const ThemeColors *tc = theme_colors();

    /* Background */
    draw_filled_rect_blend(RECT(0, 28, SIDEBAR_W, SCREEN_H - 28 - 48),
                           tc->sidebar_bg);

    /* Right border */
    draw_line(SIDEBAR_W - 1, 28, SIDEBAR_W - 1, SCREEN_H - 48,
              tc->panel_border);

    /* Icons */
    for (int i = 0; i < SIDEBAR_ITEM_COUNT; i++) {
        int iy = 40 + i * 44;
        int ix = 8;
        int icon_size = 28;

        Rect item_rect = RECT(4, iy - 4, SIDEBAR_W - 8, 36);
        int hover = rect_contains(item_rect, mx, my);

        if (hover)
            draw_filled_rounded_rect_blend(item_rect, 6,
                COLOR(255, 255, 255, 20));

        Color ic = hover ? tc->accent : tc->text_secondary;
        icon_draw(sidebar_items[i].icon, ix, iy, icon_size, ic);
    }
}

void sidebar_right_render(void)
{
    const ThemeColors *tc = theme_colors();
    int rx = SCREEN_W - RIGHT_SIDEBAR_W;

    /* Background */
    draw_filled_rect_blend(RECT(rx, 28, RIGHT_SIDEBAR_W, SCREEN_H - 28 - 48),
                           tc->sidebar_bg);

    /* Left border */
    draw_line(rx, 28, rx, SCREEN_H - 48, tc->panel_border);

    /* System info cards */
    int cy = 40;

    /* CPU card */
    widget_card(RECT(rx + 8, cy, RIGHT_SIDEBAR_W - 16, 50), 6);
    draw_text(rx + 16, cy + 6, "CPU", tc->text_muted, FONT_SIZE_SMALL);
    draw_text(rx + 16, cy + 22, "RV32IM", tc->text_primary, FONT_SIZE_SMALL);
    widget_status_dot(rx + RIGHT_SIDEBAR_W - 20, cy + 14, 4, tc->success);
    cy += 58;

    /* Memory card */
    widget_card(RECT(rx + 8, cy, RIGHT_SIDEBAR_W - 16, 60), 6);
    draw_text(rx + 16, cy + 6, "Memory", tc->text_muted, FONT_SIZE_SMALL);

    size_t free_pages = mm_free_pages();
    size_t total_pages_n = mm_total_pages();
    int mem_pct = total_pages_n > 0 ? (int)((total_pages_n - free_pages) * 100 / total_pages_n) : 0;
    char mem_str[32];
    itoa(mem_pct, mem_str, 10);
    strcat(mem_str, "%");
    draw_text(rx + 16, cy + 22, mem_str, tc->text_primary, FONT_SIZE_SMALL);
    widget_progress(RECT(rx + 16, cy + 40, RIGHT_SIDEBAR_W - 32, 8),
                    (int)(total_pages_n - free_pages), (int)total_pages_n);
    cy += 68;

    /* Tasks card */
    widget_card(RECT(rx + 8, cy, RIGHT_SIDEBAR_W - 16, 50), 6);
    draw_text(rx + 16, cy + 6, "Tasks", tc->text_muted, FONT_SIZE_SMALL);
    char task_str[16];
    itoa(sched_task_count(), task_str, 10);
    strcat(task_str, " active");
    draw_text(rx + 16, cy + 22, task_str, tc->text_primary, FONT_SIZE_SMALL);
}
