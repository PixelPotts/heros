/*
 * icons.c — Bitmap icon renderer for HerOS
 *
 * Uses pixelarticons (MIT license, https://github.com/halfmage/pixelarticons)
 * Icons are 24x24 1-bit bitmaps, rendered with nearest-neighbor scaling.
 */
#include "icons.h"
#include "draw.h"
#include "icon_data.h"

void icon_draw(IconId id, int x, int y, int size, Color c)
{
    if (id >= ICON_COUNT) return;
    const uint8_t *bmp = icon_bitmaps[id];
    if (!bmp) return;

    if (size == ICON_BITMAP_W) {
        /* 1:1 — fast path, no scaling */
        for (int row = 0; row < ICON_BITMAP_H; row++) {
            const uint8_t *rowp = bmp + row * ICON_BITMAP_STRIDE;
            for (int col = 0; col < ICON_BITMAP_W; col++) {
                if (rowp[col >> 3] & (0x80 >> (col & 7)))
                    hal_fb_pixel_blend(x + col, y + row, c);
            }
        }
    } else {
        /* Nearest-neighbor scale */
        for (int dy = 0; dy < size; dy++) {
            int sy = dy * ICON_BITMAP_H / size;
            const uint8_t *rowp = bmp + sy * ICON_BITMAP_STRIDE;
            for (int dx = 0; dx < size; dx++) {
                int sx = dx * ICON_BITMAP_W / size;
                if (rowp[sx >> 3] & (0x80 >> (sx & 7)))
                    hal_fb_pixel_blend(x + dx, y + dy, c);
            }
        }
    }
}
