#include "fb.h"
#include "bus.h"
#include "gpu_font.h"
#include <string.h>
#include <math.h>

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
    } else if (offset == FB_CTRL_DMA_SRC) {
        /* DMA blit: copy FB_SIZE bytes from guest RAM to pixel buffer */
        if (val >= RAM_BASE && val + FB_SIZE <= RAM_BASE + RAM_SIZE) {
            uint8_t *ram = bus_get_ram();
            memcpy(pixels, ram + (val - RAM_BASE), FB_SIZE);
            dirty = true;
            fb_refresh();
        }
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

uint8_t *fb_get_pixels(void)
{
    return pixels;
}

/* ── GPU 2D accelerator ─────────────────────────────────────────── */
static uint32_t gpu_regs[GPU_SIZE / 4];

uint32_t gpu_read(uint32_t offset)
{
    if (offset < GPU_SIZE)
        return gpu_regs[offset / 4];
    return 0;
}

void gpu_write(uint32_t offset, uint32_t val)
{
    if (offset >= GPU_SIZE) return;

    if (offset == GPU_CMD) {
        /* Execute command using previously written register values */
        uint32_t x     = gpu_regs[GPU_X / 4];
        uint32_t y     = gpu_regs[GPU_Y / 4];
        uint32_t w     = gpu_regs[GPU_W / 4];
        uint32_t h     = gpu_regs[GPU_H / 4];
        uint32_t color = gpu_regs[GPU_COLOR / 4];
        uint32_t src   = gpu_regs[GPU_SRC_ADDR / 4];
        uint32_t dst   = gpu_regs[GPU_DST_ADDR / 4];

        uint8_t *ram = bus_get_ram();

        switch (val) {
        case GPU_CMD_FILL_BUF: {
            /* Fill rect in a RAM-based buffer (backbuffer) */
            /* dst = RAM address of buffer start, stride = GPU_STRIDE */
            uint32_t stride = gpu_regs[GPU_STRIDE / 4];
            if (dst < RAM_BASE || !stride) break;
            uint32_t buf_off = dst - RAM_BASE;

            /* Clip to RAM bounds */
            for (uint32_t row = y; row < y + h; row++) {
                uint32_t row_start = buf_off + row * stride + x * 4;
                if (row_start + w * 4 > RAM_SIZE) break;
                uint32_t *p = (uint32_t *)(ram + row_start);
                for (uint32_t col = 0; col < w; col++)
                    p[col] = color;
            }
            break;
        }
        case GPU_CMD_BLEND_BUF: {
            /* Alpha-blend fill rect in a RAM-based buffer */
            uint32_t stride = gpu_regs[GPU_STRIDE / 4];
            if (dst < RAM_BASE || !stride) break;
            uint32_t buf_off = dst - RAM_BASE;
            uint8_t sr = (uint8_t)(color);
            uint8_t sg = (uint8_t)(color >> 8);
            uint8_t sb = (uint8_t)(color >> 16);
            uint8_t sa = (uint8_t)(color >> 24);
            uint32_t da = 255 - sa;

            for (uint32_t row = y; row < y + h; row++) {
                uint32_t row_start = buf_off + row * stride + x * 4;
                if (row_start + w * 4 > RAM_SIZE) break;
                uint8_t *p = ram + row_start;
                for (uint32_t col = 0; col < w; col++) {
                    p[0] = (uint8_t)((sr * sa + p[0] * da) / 255);
                    p[1] = (uint8_t)((sg * sa + p[1] * da) / 255);
                    p[2] = (uint8_t)((sb * sa + p[2] * da) / 255);
                    p[3] = 255;
                    p += 4;
                }
            }
            break;
        }
        case GPU_CMD_COPY: {
            /* memcpy: src→dst, length = W bytes */
            if (src >= RAM_BASE && dst >= RAM_BASE) {
                uint32_t s_off = src - RAM_BASE;
                uint32_t d_off = dst - RAM_BASE;
                if (s_off + w <= RAM_SIZE && d_off + w <= RAM_SIZE)
                    memcpy(ram + d_off, ram + s_off, w);
            }
            break;
        }
        case GPU_CMD_TEXT: {
            /* Render text string at host speed */
            uint32_t stride   = gpu_regs[GPU_STRIDE / 4];
            uint32_t str_addr = gpu_regs[GPU_STR_ADDR / 4];
            uint32_t font_idx = gpu_regs[GPU_FONT_SIZE / 4];
            if (dst < RAM_BASE || !stride) break;
            if (str_addr < RAM_BASE) break;
            if (font_idx > 2) font_idx = 1;

            const gpu_font_t *f = gpu_fonts[font_idx];
            uint32_t buf_off = dst - RAM_BASE;
            uint32_t str_off = str_addr - RAM_BASE;

            uint8_t sr = (uint8_t)(color);
            uint8_t sg = (uint8_t)(color >> 8);
            uint8_t sb = (uint8_t)(color >> 16);
            uint8_t sa = (uint8_t)(color >> 24);
            uint32_t da = 255 - sa;

            int cx = (int)x;
            /* Read null-terminated string from guest RAM */
            for (uint32_t si = str_off; si < RAM_SIZE; si++) {
                uint8_t ch = ram[si];
                if (ch == 0) break;
                if (ch == '\n') continue;
                if (ch < GPU_FONT_FIRST_CHAR || ch > GPU_FONT_LAST_CHAR) {
                    cx += f->width;
                    continue;
                }

                int idx = ch - GPU_FONT_FIRST_CHAR;
                int glyph_bytes = f->height * f->bytes_per_row;
                const uint8_t *gdata = f->data + idx * glyph_bytes;

                for (int row = 0; row < f->height; row++) {
                    int py = (int)y + row;
                    if (py < 0 || py >= FB_HEIGHT) continue;
                    const uint8_t *row_data = gdata + row * f->bytes_per_row;
                    for (int col = 0; col < f->width; col++) {
                        int px = cx + col;
                        if (px < 0 || px >= FB_WIDTH) continue;
                        int byte_idx = col / 8;
                        int bit_idx = 7 - (col % 8);
                        if (row_data[byte_idx] & (1 << bit_idx)) {
                            uint32_t off = buf_off + (uint32_t)(py * (int)(stride / 4) + px) * 4;
                            if (off + 3 >= RAM_SIZE) continue;
                            uint8_t *p = ram + off;
                            if (sa == 255) {
                                p[0] = sr; p[1] = sg; p[2] = sb; p[3] = 255;
                            } else {
                                p[0] = (uint8_t)((sr * sa + p[0] * da) / 255);
                                p[1] = (uint8_t)((sg * sa + p[1] * da) / 255);
                                p[2] = (uint8_t)((sb * sa + p[2] * da) / 255);
                                p[3] = 255;
                            }
                        }
                    }
                }
                cx += f->width;
            }
            break;
        }
        case GPU_CMD_RRECT_FILL:
        case GPU_CMD_RRECT_BLEND: {
            /* Filled rounded rectangle — scanline fill with corner indent */
            uint32_t stride = gpu_regs[GPU_STRIDE / 4];
            uint32_t radius = gpu_regs[GPU_RADIUS / 4];
            if (dst < RAM_BASE || !stride) break;
            uint32_t buf_off = dst - RAM_BASE;
            int blend = (val == GPU_CMD_RRECT_BLEND);

            uint8_t sr = (uint8_t)(color);
            uint8_t sg = (uint8_t)(color >> 8);
            uint8_t sb = (uint8_t)(color >> 16);
            uint8_t sa = (uint8_t)(color >> 24);
            uint32_t da = 255 - sa;

            int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
            int r = (int)radius;
            if (r > iw / 2) r = iw / 2;
            if (r > ih / 2) r = ih / 2;

            for (int row = 0; row < ih; row++) {
                int py = iy + row;
                if (py < 0 || py >= FB_HEIGHT) continue;

                /* Compute horizontal indent from rounded corners */
                int indent = 0;
                if (row < r) {
                    /* Top corners */
                    int dy = r - row;
                    int dx_sq = r * r - dy * dy;
                    int dx = 0;
                    while ((dx + 1) * (dx + 1) <= dx_sq) dx++;
                    indent = r - dx;
                } else if (row >= ih - r) {
                    /* Bottom corners */
                    int dy = row - (ih - 1 - r);
                    int dx_sq = r * r - dy * dy;
                    int dx = 0;
                    while ((dx + 1) * (dx + 1) <= dx_sq) dx++;
                    indent = r - dx;
                }

                int x0 = ix + indent;
                int x1 = ix + iw - indent;
                if (x0 < 0) x0 = 0;
                if (x1 > FB_WIDTH) x1 = FB_WIDTH;
                if (x0 >= x1) continue;

                uint32_t row_start = buf_off + (uint32_t)py * stride + (uint32_t)x0 * 4;
                if (row_start + (uint32_t)(x1 - x0) * 4 > RAM_SIZE) continue;

                if (blend && sa < 255) {
                    uint8_t *p = ram + row_start;
                    for (int col = x0; col < x1; col++) {
                        p[0] = (uint8_t)((sr * sa + p[0] * da) / 255);
                        p[1] = (uint8_t)((sg * sa + p[1] * da) / 255);
                        p[2] = (uint8_t)((sb * sa + p[2] * da) / 255);
                        p[3] = 255;
                        p += 4;
                    }
                } else {
                    uint32_t *p = (uint32_t *)(ram + row_start);
                    for (int col = 0; col < x1 - x0; col++)
                        p[col] = color;
                }
            }
            break;
        }
        case GPU_CMD_CIRCLE_FILL:
        case GPU_CMD_CIRCLE_BLEND: {
            /* Filled circle — scanline fill at host speed */
            uint32_t stride = gpu_regs[GPU_STRIDE / 4];
            uint32_t radius = gpu_regs[GPU_RADIUS / 4];
            if (dst < RAM_BASE || !stride) break;
            uint32_t buf_off = dst - RAM_BASE;
            int blend = (val == GPU_CMD_CIRCLE_BLEND);

            uint8_t sr = (uint8_t)(color);
            uint8_t sg = (uint8_t)(color >> 8);
            uint8_t sb = (uint8_t)(color >> 16);
            uint8_t sa = (uint8_t)(color >> 24);
            uint32_t da = 255 - sa;

            int cx = (int)x, cy = (int)y, r = (int)radius;
            int r2 = r * r;

            for (int dy = -r; dy <= r; dy++) {
                int py = cy + dy;
                if (py < 0 || py >= FB_HEIGHT) continue;

                /* Integer sqrt for span width */
                int rem = r2 - dy * dy;
                int dx = 0;
                while ((dx + 1) * (dx + 1) <= rem) dx++;

                int x0 = cx - dx;
                int x1 = cx + dx + 1;
                if (x0 < 0) x0 = 0;
                if (x1 > FB_WIDTH) x1 = FB_WIDTH;
                if (x0 >= x1) continue;

                uint32_t row_start = buf_off + (uint32_t)py * stride + (uint32_t)x0 * 4;
                if (row_start + (uint32_t)(x1 - x0) * 4 > RAM_SIZE) continue;

                if (blend && sa < 255) {
                    uint8_t *p = ram + row_start;
                    for (int col = x0; col < x1; col++) {
                        p[0] = (uint8_t)((sr * sa + p[0] * da) / 255);
                        p[1] = (uint8_t)((sg * sa + p[1] * da) / 255);
                        p[2] = (uint8_t)((sb * sa + p[2] * da) / 255);
                        p[3] = 255;
                        p += 4;
                    }
                } else {
                    uint32_t *p = (uint32_t *)(ram + row_start);
                    for (int col = 0; col < x1 - x0; col++)
                        p[col] = color;
                }
            }
            break;
        }
        case GPU_CMD_LINE: {
            /* Bresenham line with alpha blend at host speed */
            uint32_t stride = gpu_regs[GPU_STRIDE / 4];
            uint32_t x1_reg = gpu_regs[GPU_X1 / 4];
            uint32_t y1_reg = gpu_regs[GPU_Y1 / 4];
            if (dst < RAM_BASE || !stride) break;
            uint32_t buf_off = dst - RAM_BASE;

            uint8_t sr = (uint8_t)(color);
            uint8_t sg = (uint8_t)(color >> 8);
            uint8_t sb = (uint8_t)(color >> 16);
            uint8_t sa = (uint8_t)(color >> 24);
            uint32_t da = 255 - sa;

            int lx0 = (int)x, ly0 = (int)y;
            int lx1 = (int)x1_reg, ly1 = (int)y1_reg;

            int ldx = lx1 - lx0;
            int ldy = ly1 - ly0;
            int sx = (ldx > 0) ? 1 : -1;
            int sy = (ldy > 0) ? 1 : -1;
            if (ldx < 0) ldx = -ldx;
            if (ldy < 0) ldy = -ldy;

            #define GPU_LINE_PIXEL(px, py) do { \
                if ((px) >= 0 && (px) < FB_WIDTH && (py) >= 0 && (py) < FB_HEIGHT) { \
                    uint32_t off = buf_off + (uint32_t)(py) * stride + (uint32_t)(px) * 4; \
                    if (off + 3 < RAM_SIZE) { \
                        uint8_t *p = ram + off; \
                        if (sa == 255) { \
                            p[0] = sr; p[1] = sg; p[2] = sb; p[3] = 255; \
                        } else { \
                            p[0] = (uint8_t)((sr * sa + p[0] * da) / 255); \
                            p[1] = (uint8_t)((sg * sa + p[1] * da) / 255); \
                            p[2] = (uint8_t)((sb * sa + p[2] * da) / 255); \
                            p[3] = 255; \
                        } \
                    } \
                } \
            } while(0)

            if (ldx >= ldy) {
                int err = ldx / 2;
                int cy = ly0;
                for (int cx = lx0; cx != lx1 + sx; cx += sx) {
                    GPU_LINE_PIXEL(cx, cy);
                    err -= ldy;
                    if (err < 0) { cy += sy; err += ldx; }
                }
            } else {
                int err = ldy / 2;
                int cx = lx0;
                for (int cy = ly0; cy != ly1 + sy; cy += sy) {
                    GPU_LINE_PIXEL(cx, cy);
                    err -= ldx;
                    if (err < 0) { cx += sx; err += ldy; }
                }
            }
            #undef GPU_LINE_PIXEL
            break;
        }
        default:
            break;
        }
    } else {
        gpu_regs[offset / 4] = val;
    }
}
