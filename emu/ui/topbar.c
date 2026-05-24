#include "types.h"
#include "draw.h"
#include "theme.h"
#include "systray.h"

#define TOPBAR_H  28

void topbar_render(void)
{
    const ThemeColors *tc = theme_colors();

    /* Background */
    draw_filled_rect_blend(RECT(0, 0, SCREEN_W, TOPBAR_H), tc->topbar_bg);

    /* Bottom border */
    draw_line(0, TOPBAR_H - 1, SCREEN_W, TOPBAR_H - 1, tc->panel_border);

    /* Brand text "HER" */
    draw_text(10, 8, "HER", tc->accent, FONT_SIZE_SMALL);

    /* System tray on right */
    systray_render(RECT(SCREEN_W - 200, 0, 200, TOPBAR_H));
}
