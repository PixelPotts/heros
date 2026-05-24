#ifndef HAL_TEXT_H
#define HAL_TEXT_H

#include "hal.h"
#include "font_data.h"

/* Font size enum for convenience */
typedef enum {
    FONT_SIZE_SMALL  = 0,
    FONT_SIZE_MEDIUM = 1,
    FONT_SIZE_LARGE  = 2,
    FONT_SIZE_COUNT  = 3
} hal_font_size_t;

/* Get font struct for a size */
const font_t *hal_get_font(hal_font_size_t size);

/* Render text */
void hal_text(int x, int y, const char *text, hal_color_t c, hal_font_size_t size);
void hal_text_centered(int cx, int y, const char *text, hal_color_t c, hal_font_size_t size);
void hal_text_right(int right_x, int y, const char *text, hal_color_t c, hal_font_size_t size);

/* Measure text */
int hal_text_width(const char *text, hal_font_size_t size);
int hal_text_height(hal_font_size_t size);

/* Render with letter spacing */
void hal_text_spaced(int x, int y, const char *text, hal_color_t c,
                      hal_font_size_t size, int extra_spacing);

#endif
