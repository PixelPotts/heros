#include "fb.h"
#include <string.h>

static uint8_t       pixels[FB_SIZE];
static SDL_Texture  *texture;
static SDL_Renderer *renderer;
static bool          dirty;

void fb_init(SDL_Renderer *rend)
{
    renderer = rend;
    memset(pixels, 0, FB_SIZE);
    dirty = true;

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ABGR8888,  /* matches RGBA byte order */
                                SDL_TEXTUREACCESS_STREAMING,
                                FB_WIDTH, FB_HEIGHT);
}

uint8_t fb_read(uint32_t addr)
{
    if (addr < FB_SIZE)
        return pixels[addr];
    return 0;
}

void fb_write(uint32_t addr, uint8_t val)
{
    if (addr < FB_SIZE) {
        pixels[addr] = val;
        dirty = true;
    }
}

uint32_t fb_ctrl_read(uint32_t offset)
{
    switch (offset) {
    case FB_CTRL_WIDTH:   return FB_WIDTH;
    case FB_CTRL_HEIGHT:  return FB_HEIGHT;
    default:              return 0;
    }
}

void fb_ctrl_write(uint32_t offset, uint32_t val)
{
    if (offset == FB_CTRL_FLUSH && val == 1) {
        fb_refresh();
    }
}

void fb_refresh(void)
{
    if (!texture || !renderer) return;

    SDL_UpdateTexture(texture, NULL, pixels, FB_WIDTH * FB_BPP);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    dirty = false;
}

bool fb_is_dirty(void)
{
    return dirty;
}

void fb_cleanup(void)
{
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
}
