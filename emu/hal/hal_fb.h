#ifndef HAL_FB_H
#define HAL_FB_H

#include "hal.h"

/* Screen dimensions */
#define HAL_SCREEN_W  1280
#define HAL_SCREEN_H  720

void hal_fb_init(void);
void hal_fb_flush(void);

/* Pixel operations */
void hal_fb_pixel(int x, int y, hal_color_t c);
void hal_fb_pixel_blend(int x, int y, hal_color_t c);

/* Rectangle operations */
void hal_fb_fill_rect(hal_rect_t r, hal_color_t c);
void hal_fb_fill_rect_blend(hal_rect_t r, hal_color_t c);
void hal_fb_hline(int x, int y, int w, hal_color_t c);
void hal_fb_vline(int x, int y, int h, hal_color_t c);

/* Blit from buffer */
void hal_fb_blit(int dx, int dy, int w, int h,
                  const uint8_t *src, int src_stride);

/* Clear screen */
void hal_fb_clear(hal_color_t c);

/* Get raw buffer pointer (for direct manipulation) */
uint8_t *hal_fb_buffer(void);

#endif
