#include "hal_fb.h"
#include "../kernel/fb.h"
#include "../kernel/string.h"

void hal_fb_init(void)
{
    /* fb_driver_init already called from kmain */
}

void hal_fb_flush(void)
{
    fb_driver_flush();
}

void hal_fb_pixel(int x, int y, hal_color_t c)
{
    if (x < 0 || x >= HAL_SCREEN_W || y < 0 || y >= HAL_SCREEN_H) return;
    uint8_t *buf = fb_get_backbuffer();
    if (!buf) return;
    uint32_t off = (y * HAL_SCREEN_W + x) * 4;
    buf[off + 0] = c.r;
    buf[off + 1] = c.g;
    buf[off + 2] = c.b;
    buf[off + 3] = c.a;
}

void hal_fb_pixel_blend(int x, int y, hal_color_t c)
{
    if (x < 0 || x >= HAL_SCREEN_W || y < 0 || y >= HAL_SCREEN_H) return;
    if (c.a == 0) return;
    if (c.a == 255) { hal_fb_pixel(x, y, c); return; }

    uint8_t *buf = fb_get_backbuffer();
    if (!buf) return;
    uint32_t off = (y * HAL_SCREEN_W + x) * 4;

    uint32_t sa = c.a;
    uint32_t da = 255 - sa;
    buf[off + 0] = (uint8_t)((c.r * sa + buf[off + 0] * da) / 255);
    buf[off + 1] = (uint8_t)((c.g * sa + buf[off + 1] * da) / 255);
    buf[off + 2] = (uint8_t)((c.b * sa + buf[off + 2] * da) / 255);
    buf[off + 3] = 255;
}

void hal_fb_fill_rect(hal_rect_t r, hal_color_t c)
{
    /* Clip */
    int x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > HAL_SCREEN_W) x1 = HAL_SCREEN_W;
    if (y1 > HAL_SCREEN_H) y1 = HAL_SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;

    uint8_t *buf = fb_get_backbuffer();
    if (!buf) return;

    for (int y = y0; y < y1; y++) {
        uint8_t *row = buf + (y * HAL_SCREEN_W + x0) * 4;
        for (int x = x0; x < x1; x++) {
            row[0] = c.r;
            row[1] = c.g;
            row[2] = c.b;
            row[3] = c.a;
            row += 4;
        }
    }
}

void hal_fb_fill_rect_blend(hal_rect_t r, hal_color_t c)
{
    if (c.a == 255) { hal_fb_fill_rect(r, c); return; }
    if (c.a == 0) return;

    int x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > HAL_SCREEN_W) x1 = HAL_SCREEN_W;
    if (y1 > HAL_SCREEN_H) y1 = HAL_SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;

    uint8_t *buf = fb_get_backbuffer();
    if (!buf) return;

    uint32_t sa = c.a;
    uint32_t da = 255 - sa;

    for (int y = y0; y < y1; y++) {
        uint8_t *row = buf + (y * HAL_SCREEN_W + x0) * 4;
        for (int x = x0; x < x1; x++) {
            row[0] = (uint8_t)((c.r * sa + row[0] * da) / 255);
            row[1] = (uint8_t)((c.g * sa + row[1] * da) / 255);
            row[2] = (uint8_t)((c.b * sa + row[2] * da) / 255);
            row[3] = 255;
            row += 4;
        }
    }
}

void hal_fb_hline(int x, int y, int w, hal_color_t c)
{
    hal_fb_fill_rect(hal_rect(x, y, w, 1), c);
}

void hal_fb_vline(int x, int y, int h, hal_color_t c)
{
    hal_fb_fill_rect(hal_rect(x, y, 1, h), c);
}

void hal_fb_blit(int dx, int dy, int w, int h,
                  const uint8_t *src, int src_stride)
{
    uint8_t *buf = fb_get_backbuffer();
    if (!buf) return;

    for (int row = 0; row < h; row++) {
        int sy = dy + row;
        if (sy < 0 || sy >= HAL_SCREEN_H) continue;
        const uint8_t *sp = src + row * src_stride;
        uint8_t *dp = buf + (sy * HAL_SCREEN_W + dx) * 4;

        int sx_start = 0;
        int sx_end = w;
        if (dx < 0) sx_start = -dx;
        if (dx + w > HAL_SCREEN_W) sx_end = HAL_SCREEN_W - dx;

        if (sx_start < sx_end)
            memcpy(dp + sx_start * 4, sp + sx_start * 4, (sx_end - sx_start) * 4);
    }
}

void hal_fb_clear(hal_color_t c)
{
    hal_fb_fill_rect(hal_rect(0, 0, HAL_SCREEN_W, HAL_SCREEN_H), c);
}

uint8_t *hal_fb_buffer(void)
{
    return fb_get_backbuffer();
}
