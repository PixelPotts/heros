#include "lockscreen.h"
#include "draw.h"
#include "theme.h"
#include "icons.h"
#include "systray.h"
#include "../kernel/string.h"

#define MAX_PASSWORD  32
#define PASSWORD      "heros"   /* default password */

static int  locked = 1;
static char input_buf[MAX_PASSWORD + 1];
static int  input_len = 0;
static int  error_flash = 0;
static uint32_t error_time = 0;

void lockscreen_init(void)
{
    locked = 1;
    input_len = 0;
    input_buf[0] = '\0';
    error_flash = 0;
}

void lockscreen_render(void)
{
    if (!locked) return;

    const ThemeColors *tc = theme_colors();

    /* Dark overlay */
    draw_filled_rect_blend(RECT(0, 0, SCREEN_W, SCREEN_H),
                           COLOR(0, 0, 0, 200));

    /* Clock */
    char time_str[16];
    systray_get_time_string(time_str, sizeof(time_str));
    draw_text_centered(SCREEN_W / 2, SCREEN_H / 3 - 40,
                       time_str, tc->text_primary, FONT_SIZE_LARGE);

    /* Lock icon */
    icon_draw(ICON_LOCK, SCREEN_W / 2 - 16, SCREEN_H / 3 + 10, 32,
              tc->text_secondary);

    /* "HerOS" text */
    draw_text_centered(SCREEN_W / 2, SCREEN_H / 3 + 55,
                       "HerOS", tc->accent, FONT_SIZE_MEDIUM);

    /* Password field */
    int field_w = 200;
    int field_h = 30;
    int field_x = SCREEN_W / 2 - field_w / 2;
    int field_y = SCREEN_H / 2 + 20;

    Color field_bg = error_flash ? COLOR(80, 20, 20, 200) : COLOR(30, 30, 50, 200);
    draw_filled_rounded_rect_blend(RECT(field_x, field_y, field_w, field_h),
                                    6, field_bg);
    draw_rounded_rect(RECT(field_x, field_y, field_w, field_h), 6,
                      error_flash ? tc->error : tc->panel_border);

    /* Password dots */
    for (int i = 0; i < input_len; i++) {
        draw_filled_circle(field_x + 20 + i * 14, field_y + field_h / 2,
                           3, tc->text_primary);
    }

    /* Cursor */
    if ((hal_get_ticks() / 500) % 2 == 0) {
        int cx = field_x + 20 + input_len * 14;
        draw_line(cx, field_y + 6, cx, field_y + field_h - 6, tc->accent);
    }

    /* Hint text */
    draw_text_centered(SCREEN_W / 2, field_y + field_h + 12,
                       "Enter password to unlock",
                       tc->text_muted, FONT_SIZE_SMALL);

    /* Clear error flash after 500ms */
    if (error_flash && hal_get_ticks() - error_time > 500)
        error_flash = 0;
}

int lockscreen_handle_key(uint16_t key, uint16_t mod)
{
    (void)mod;
    if (!locked) return 0;

    if (key == HAL_KEY_RETURN) {
        /* Check password */
        if (strcmp(input_buf, PASSWORD) == 0) {
            locked = 0;
            input_len = 0;
            input_buf[0] = '\0';
            return 1;  /* unlocked */
        } else {
            error_flash = 1;
            error_time = hal_get_ticks();
            input_len = 0;
            input_buf[0] = '\0';
        }
    } else if (key == HAL_KEY_BACKSPACE) {
        if (input_len > 0) {
            input_len--;
            input_buf[input_len] = '\0';
        }
    }
    return 0;
}

int lockscreen_handle_text(char ch)
{
    if (!locked) return 0;
    if (ch >= 32 && ch < 127 && input_len < MAX_PASSWORD) {
        input_buf[input_len++] = ch;
        input_buf[input_len] = '\0';
    }
    return 0;
}

int lockscreen_is_locked(void)
{
    return locked;
}

void lockscreen_lock(void)
{
    locked = 1;
    input_len = 0;
    input_buf[0] = '\0';
}
