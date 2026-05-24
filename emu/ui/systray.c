#include "systray.h"
#include "draw.h"
#include "theme.h"
#include "icons.h"
#include "../kernel/kprintf.h"

void systray_get_time_string(char *buf, int buf_size)
{
    uint32_t ms = hal_get_ticks();
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;

    /* Display as HH:MM:SS since boot */
    hours %= 24;
    mins %= 60;
    secs %= 60;

    /* Manual snprintf since we have no libc */
    if (buf_size < 9) { buf[0] = '\0'; return; }
    buf[0] = '0' + (hours / 10);
    buf[1] = '0' + (hours % 10);
    buf[2] = ':';
    buf[3] = '0' + (mins / 10);
    buf[4] = '0' + (mins % 10);
    buf[5] = ':';
    buf[6] = '0' + (secs / 10);
    buf[7] = '0' + (secs % 10);
    buf[8] = '\0';
}

void systray_render(Rect area)
{
    const ThemeColors *tc = theme_colors();

    /* Battery icon */
    icon_draw(ICON_BATTERY, area.x + area.w - 80, area.y + 4, 16, tc->text_secondary);

    /* WiFi icon */
    icon_draw(ICON_WIFI, area.x + area.w - 56, area.y + 4, 16, tc->accent);

    /* Clock */
    char time_str[16];
    systray_get_time_string(time_str, sizeof(time_str));
    draw_text(area.x + area.w - 36, area.y + 7, time_str,
              tc->text_primary, FONT_SIZE_SMALL);
}
