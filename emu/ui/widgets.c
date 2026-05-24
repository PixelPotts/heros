#include "widgets.h"
#include "draw.h"
#include "theme.h"

void widget_card(Rect r, int radius)
{
    const ThemeColors *tc = theme_colors();
    draw_filled_rounded_rect_blend(r, radius, tc->panel_bg);
    draw_rounded_rect(r, radius, tc->panel_border);
}

int widget_button(Rect r, const char *label, ButtonStyle style,
                   int mx, int my, int pressed)
{
    const ThemeColors *tc = theme_colors();
    int hover = rect_contains(r, mx, my);
    Color bg, fg;

    switch (style) {
    case BTN_PRIMARY:
        bg = hover ? tc->accent_hover : tc->accent;
        fg = COLOR_WHITE;
        break;
    case BTN_SECONDARY:
        bg = hover ? tc->panel_highlight : tc->panel_bg;
        fg = tc->text_primary;
        break;
    case BTN_GHOST:
        bg = hover ? COLOR(255, 255, 255, 20) : COLOR_TRANSPARENT;
        fg = tc->text_secondary;
        break;
    case BTN_DANGER:
        bg = hover ? COLOR(240, 80, 80, 255) : tc->error;
        fg = COLOR_WHITE;
        break;
    default:
        bg = tc->panel_bg;
        fg = tc->text_primary;
    }

    if (pressed && hover) {
        bg.r = bg.r > 20 ? bg.r - 20 : 0;
        bg.g = bg.g > 20 ? bg.g - 20 : 0;
        bg.b = bg.b > 20 ? bg.b - 20 : 0;
    }

    draw_filled_rounded_rect_blend(r, 4, bg);
    draw_text_centered(r.x + r.w / 2, r.y + (r.h - 10) / 2,
                       label, fg, FONT_SIZE_SMALL);
    return hover;
}

void widget_toggle(int x, int y, int on)
{
    const ThemeColors *tc = theme_colors();
    int w = 36, h = 18;
    Color bg = on ? tc->accent : tc->panel_bg;
    draw_filled_rounded_rect(RECT(x, y, w, h), h/2, bg);
    draw_rounded_rect(RECT(x, y, w, h), h/2, tc->panel_border);

    int knob_x = on ? x + w - h + 2 : x + 2;
    draw_filled_circle(knob_x + h/2 - 2, y + h/2, h/2 - 3, COLOR_WHITE);
}

void widget_progress(Rect r, int value, int max_val)
{
    const ThemeColors *tc = theme_colors();
    draw_filled_rounded_rect(r, 3, tc->panel_bg);

    if (max_val > 0 && value > 0) {
        int fill_w = r.w * value / max_val;
        if (fill_w > r.w) fill_w = r.w;
        draw_filled_rounded_rect(RECT(r.x, r.y, fill_w, r.h), 3, tc->accent);
    }
}

void widget_badge(int x, int y, const char *text, Color bg)
{
    int w = draw_text_width(text, FONT_SIZE_SMALL) + 10;
    draw_filled_rounded_rect(RECT(x, y, w, 16), 8, bg);
    draw_text(x + 5, y + 3, text, COLOR_WHITE, FONT_SIZE_SMALL);
}

void widget_separator(int x, int y, int w)
{
    const ThemeColors *tc = theme_colors();
    draw_line(x, y, x + w, y, tc->panel_border);
}

void widget_section_header(int x, int y, const char *text)
{
    const ThemeColors *tc = theme_colors();
    draw_text(x, y, text, tc->text_muted, FONT_SIZE_SMALL);
}

void widget_status_dot(int x, int y, int radius, Color c)
{
    draw_filled_circle(x, y, radius, c);
}

void widget_list_item(Rect r, const char *text, const char *right_text,
                       int selected, int mx, int my)
{
    const ThemeColors *tc = theme_colors();
    int hover = rect_contains(r, mx, my);

    if (selected)
        draw_filled_rounded_rect_blend(r, 4, tc->accent_dim);
    else if (hover)
        draw_filled_rounded_rect_blend(r, 4, COLOR(255, 255, 255, 15));

    draw_text(r.x + 8, r.y + (r.h - 10) / 2, text,
              selected ? COLOR_WHITE : tc->text_primary, FONT_SIZE_SMALL);

    if (right_text)
        draw_text_right(r.x + r.w - 8, r.y + (r.h - 10) / 2,
                        right_text, tc->text_muted, FONT_SIZE_SMALL);
}
