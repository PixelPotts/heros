#include "hal_text.h"
#include "hal_fb.h"

static const font_t *fonts[FONT_SIZE_COUNT];

static void ensure_fonts(void)
{
    if (!fonts[0]) {
        fonts[FONT_SIZE_SMALL]  = &font_small;
        fonts[FONT_SIZE_MEDIUM] = &font_medium;
        fonts[FONT_SIZE_LARGE]  = &font_large;
    }
}

const font_t *hal_get_font(hal_font_size_t size)
{
    ensure_fonts();
    if (size >= FONT_SIZE_COUNT) size = FONT_SIZE_MEDIUM;
    return fonts[size];
}

static void render_glyph(int x, int y, char ch, hal_color_t c, const font_t *f)
{
    if (ch < FONT_FIRST_CHAR || ch > FONT_LAST_CHAR) return;

    int idx = ch - FONT_FIRST_CHAR;
    int glyph_bytes = f->height * f->bytes_per_row;
    const uint8_t *gdata = f->data + idx * glyph_bytes;

    for (int row = 0; row < f->height; row++) {
        const uint8_t *row_data = gdata + row * f->bytes_per_row;
        for (int col = 0; col < f->width; col++) {
            int byte_idx = col / 8;
            int bit_idx = 7 - (col % 8);
            if (row_data[byte_idx] & (1 << bit_idx)) {
                hal_fb_pixel_blend(x + col, y + row, c);
            }
        }
    }
}

void hal_text(int x, int y, const char *text, hal_color_t c, hal_font_size_t size)
{
    const font_t *f = hal_get_font(size);
    while (*text) {
        if (*text == '\n') {
            /* Skip newlines — caller should handle multi-line */
            text++;
            continue;
        }
        render_glyph(x, y, *text, c, f);
        x += f->width;
        text++;
    }
}

void hal_text_centered(int cx, int y, const char *text, hal_color_t c, hal_font_size_t size)
{
    int w = hal_text_width(text, size);
    hal_text(cx - w / 2, y, text, c, size);
}

void hal_text_right(int right_x, int y, const char *text, hal_color_t c, hal_font_size_t size)
{
    int w = hal_text_width(text, size);
    hal_text(right_x - w, y, text, c, size);
}

int hal_text_width(const char *text, hal_font_size_t size)
{
    const font_t *f = hal_get_font(size);
    int len = 0;
    while (*text) {
        if (*text != '\n') len++;
        text++;
    }
    return len * f->width;
}

int hal_text_height(hal_font_size_t size)
{
    const font_t *f = hal_get_font(size);
    return f->height;
}

void hal_text_spaced(int x, int y, const char *text, hal_color_t c,
                      hal_font_size_t size, int extra_spacing)
{
    const font_t *f = hal_get_font(size);
    while (*text) {
        if (*text != '\n') {
            render_glyph(x, y, *text, c, f);
            x += f->width + extra_spacing;
        }
        text++;
    }
}
