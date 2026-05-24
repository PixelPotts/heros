#ifndef FB_H
#define FB_H

#include "emu.h"
#include <SDL2/SDL.h>

void     fb_init(SDL_Renderer *renderer);
uint8_t  fb_read(uint32_t addr);          /* addr relative to FB_BASE */
void     fb_write(uint32_t addr, uint8_t val);
uint32_t fb_ctrl_read(uint32_t offset);
void     fb_ctrl_write(uint32_t offset, uint32_t val);
void     fb_refresh(void);                /* push pixels to SDL */
bool     fb_is_dirty(void);
void     fb_cleanup(void);

/* GPU 2D accelerator */
uint32_t gpu_read(uint32_t offset);
void     gpu_write(uint32_t offset, uint32_t val);
uint8_t *fb_get_pixels(void);             /* host pixel buffer */

#endif
