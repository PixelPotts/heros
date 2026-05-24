#ifndef KERNEL_FB_H
#define KERNEL_FB_H

#include <stdint.h>

#define FB_WIDTH    1280
#define FB_HEIGHT   720
#define FB_BPP      4
#define FB_STRIDE   (FB_WIDTH * FB_BPP)
#define FB_SIZE     (FB_WIDTH * FB_HEIGHT * FB_BPP)

void     fb_driver_init(void);
void     fb_driver_flush(void);
uint8_t *fb_get_backbuffer(void);

/* Direct pixel access on backbuffer */
void fb_driver_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void fb_driver_fill_rect(int x, int y, int w, int h,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif
