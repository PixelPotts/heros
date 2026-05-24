/*
 * font_data.h - Embedded bitmap font data for HerOS
 *
 * Three font sizes with ASCII glyphs 32-126:
 *   font_small  : 6x10 pixels (labels, status text)
 *   font_medium : 8x14 pixels (body text, UI elements)
 *   font_large  : 10x18 pixels (titles, headings)
 *
 * Storage: 1bpp packed, MSB first, rows padded to byte boundary.
 */

#ifndef FONT_DATA_H
#define FONT_DATA_H

#include <stdint.h>

#define FONT_FIRST_CHAR  32
#define FONT_LAST_CHAR   126
#define FONT_NUM_GLYPHS  95

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t bytes_per_row;
    const uint8_t *data;  /* glyph data array, each glyph is height * bytes_per_row bytes */
} font_t;

extern const font_t font_small;   /* 6x10  */
extern const font_t font_medium;  /* 8x14  */
extern const font_t font_large;   /* 10x18 */

#endif /* FONT_DATA_H */
