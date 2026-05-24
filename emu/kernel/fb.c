#include "fb.h"
#include "mm.h"
#include "string.h"
#include "kprintf.h"

/* Framebuffer MMIO addresses */
#define FB_MMIO_BASE    0x20000000
#define FB_CTRL_FLUSH   (*(volatile uint32_t *)0x21000008)

/* Back buffer in RAM (allocated via page_alloc) */
static uint8_t *backbuffer;

void fb_driver_init(void)
{
    /* Allocate back buffer: 1280*720*4 = 3,686,400 bytes = 900 pages */
    size_t pages = (FB_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    backbuffer = (uint8_t *)page_alloc(pages);
    if (!backbuffer) {
        kpanic("Cannot allocate framebuffer backbuffer (%u pages)\n",
               (unsigned)pages);
    }
    memset(backbuffer, 0, FB_SIZE);
    kprintf("[fb] Backbuffer at 0x%08x (%u x %u, %u bytes)\n",
            (uint32_t)(uintptr_t)backbuffer, FB_WIDTH, FB_HEIGHT, FB_SIZE);
}

void fb_driver_flush(void)
{
    if (!backbuffer) return;

    /* Copy backbuffer to MMIO framebuffer */
    volatile uint32_t *dst = (volatile uint32_t *)FB_MMIO_BASE;
    const uint32_t *src = (const uint32_t *)backbuffer;
    uint32_t count = FB_SIZE / 4;

    for (uint32_t i = 0; i < count; i++)
        dst[i] = src[i];

    /* Signal the emulator to refresh */
    FB_CTRL_FLUSH = 1;
}

uint8_t *fb_get_backbuffer(void)
{
    return backbuffer;
}

void fb_driver_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    if (!backbuffer) return;
    uint32_t off = (y * FB_WIDTH + x) * FB_BPP;
    backbuffer[off + 0] = r;
    backbuffer[off + 1] = g;
    backbuffer[off + 2] = b;
    backbuffer[off + 3] = a;
}

void fb_driver_fill_rect(int x, int y, int w, int h,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    /* Clip */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > FB_WIDTH)  w = FB_WIDTH - x;
    if (y + h > FB_HEIGHT) h = FB_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    if (!backbuffer) return;

    for (int row = y; row < y + h; row++) {
        uint8_t *p = backbuffer + (row * FB_WIDTH + x) * FB_BPP;
        for (int col = 0; col < w; col++) {
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
            p += FB_BPP;
        }
    }
}
