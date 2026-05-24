#include "hal_draw.h"
#include "hal_fb.h"

/* ── Sin lookup table (256 entries, Q8 fixed-point) ──────────── */
/* sin(i * 2*pi / 256) * 256, for i = 0..63 (first quadrant) */
static const int16_t sin_table[65] = {
      0,   6,  13,  19,  25,  31,  37,  44,
     50,  56,  62,  68,  74,  80,  86,  92,
     98, 103, 109, 115, 120, 126, 131, 136,
    142, 147, 152, 157, 162, 167, 171, 176,
    181, 185, 189, 193, 197, 201, 205, 209,
    212, 216, 219, 222, 225, 228, 231, 234,
    236, 238, 241, 243, 244, 246, 248, 249,
    251, 252, 253, 254, 254, 255, 255, 256,
    256
};

int hal_sin256(int angle)
{
    angle = angle & 255;
    if (angle < 64)  return  sin_table[angle];
    if (angle < 128) return  sin_table[128 - angle];
    if (angle < 192) return -sin_table[angle - 128];
    return -sin_table[256 - angle];
}

int hal_cos256(int angle)
{
    return hal_sin256(angle + 64);
}

unsigned int hal_isqrt(unsigned int n)
{
    if (n == 0) return 0;
    unsigned int x = n;
    unsigned int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/* ── Bresenham line ──────────────────────────────────────────── */
void hal_draw_line(int x0, int y0, int x1, int y1, hal_color_t c)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    if (dx >= dy) {
        int err = dx / 2;
        int y = y0;
        for (int x = x0; x != x1 + sx; x += sx) {
            hal_fb_pixel_blend(x, y, c);
            err -= dy;
            if (err < 0) {
                y += sy;
                err += dx;
            }
        }
    } else {
        int err = dy / 2;
        int x = x0;
        for (int y = y0; y != y1 + sy; y += sy) {
            hal_fb_pixel_blend(x, y, c);
            err -= dx;
            if (err < 0) {
                x += sx;
                err += dy;
            }
        }
    }
}

/* ── Rectangle ───────────────────────────────────────────────── */
void hal_draw_rect(hal_rect_t r, hal_color_t c)
{
    hal_fb_hline(r.x, r.y, r.w, c);
    hal_fb_hline(r.x, r.y + r.h - 1, r.w, c);
    hal_fb_vline(r.x, r.y, r.h, c);
    hal_fb_vline(r.x + r.w - 1, r.y, r.h, c);
}

void hal_draw_filled_rect(hal_rect_t r, hal_color_t c)
{
    hal_fb_fill_rect(r, c);
}

void hal_draw_filled_rect_blend(hal_rect_t r, hal_color_t c)
{
    hal_fb_fill_rect_blend(r, c);
}

/* ── Rounded rectangle ──────────────────────────────────────── */
static void __attribute__((unused)) draw_rounded_corners(int cx, int cy, int radius,
                                   int x0, int y0, int x1, int y1,
                                   hal_color_t c, int filled, int blend)
{
    /* Midpoint circle for the 4 corners */
    int x = 0, y = radius, d = 1 - radius;

    while (x <= y) {
        if (filled) {
            /* Top-left */
            int lx, rx, ty;
            /* Horizontal spans for each octant pair */

            /* Top portion: cy - y row, cx - x to cx + x */
            ty = cy - y;
            if (ty >= y0 && ty < y1) {
                lx = cx - x; rx = cx + x;
                if (blend)
                    hal_fb_fill_rect_blend(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
                else
                    hal_fb_fill_rect(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
            }
            ty = cy + y;
            if (ty >= y0 && ty < y1) {
                lx = cx - x; rx = cx + x;
                if (blend)
                    hal_fb_fill_rect_blend(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
                else
                    hal_fb_fill_rect(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
            }
            ty = cy - x;
            if (ty >= y0 && ty < y1 && x != y) {
                lx = cx - y; rx = cx + y;
                if (blend)
                    hal_fb_fill_rect_blend(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
                else
                    hal_fb_fill_rect(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
            }
            ty = cy + x;
            if (ty >= y0 && ty < y1 && x != y) {
                lx = cx - y; rx = cx + y;
                if (blend)
                    hal_fb_fill_rect_blend(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
                else
                    hal_fb_fill_rect(hal_rect(x0 > lx ? x0 : lx, ty,
                        ((x1 < rx+1 ? x1 : rx+1) - (x0 > lx ? x0 : lx)), 1), c);
            }
        } else {
            /* Outline only */
            hal_fb_pixel_blend(cx + x, cy + y, c);
            hal_fb_pixel_blend(cx - x, cy + y, c);
            hal_fb_pixel_blend(cx + x, cy - y, c);
            hal_fb_pixel_blend(cx - x, cy - y, c);
            hal_fb_pixel_blend(cx + y, cy + x, c);
            hal_fb_pixel_blend(cx - y, cy + x, c);
            hal_fb_pixel_blend(cx + y, cy - x, c);
            hal_fb_pixel_blend(cx - y, cy - x, c);
        }

        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

void hal_draw_filled_rounded_rect(hal_rect_t r, int radius, hal_color_t c)
{
    if (radius <= 0) { hal_fb_fill_rect(r, c); return; }
    if (radius > r.w / 2) radius = r.w / 2;
    if (radius > r.h / 2) radius = r.h / 2;

    /* Center rectangle */
    hal_fb_fill_rect(hal_rect(r.x + radius, r.y, r.w - 2 * radius, r.h), c);
    /* Left strip */
    hal_fb_fill_rect(hal_rect(r.x, r.y + radius, radius, r.h - 2 * radius), c);
    /* Right strip */
    hal_fb_fill_rect(hal_rect(r.x + r.w - radius, r.y + radius, radius, r.h - 2 * radius), c);

    /* Four corner quarter-circles */
    /* Top-left */
    int cx, cy;
    cx = r.x + radius; cy = r.y + radius;
    for (int py = 0; py < radius; py++) {
        for (int px = 0; px < radius; px++) {
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel(cx - radius + px, cy - radius + py, c);
        }
    }
    /* Top-right */
    cx = r.x + r.w - radius - 1; cy = r.y + radius;
    for (int py = 0; py < radius; py++) {
        for (int px = 0; px < radius; px++) {
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel(cx + radius - px, cy - radius + py, c);
        }
    }
    /* Bottom-left */
    cx = r.x + radius; cy = r.y + r.h - radius - 1;
    for (int py = 0; py < radius; py++) {
        for (int px = 0; px < radius; px++) {
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel(cx - radius + px, cy + radius - py, c);
        }
    }
    /* Bottom-right */
    cx = r.x + r.w - radius - 1; cy = r.y + r.h - radius - 1;
    for (int py = 0; py < radius; py++) {
        for (int px = 0; px < radius; px++) {
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel(cx + radius - px, cy + radius - py, c);
        }
    }
}

void hal_draw_filled_rounded_rect_blend(hal_rect_t r, int radius, hal_color_t c)
{
    if (c.a == 255) { hal_draw_filled_rounded_rect(r, radius, c); return; }
    if (c.a == 0) return;
    if (radius <= 0) { hal_fb_fill_rect_blend(r, c); return; }
    if (radius > r.w / 2) radius = r.w / 2;
    if (radius > r.h / 2) radius = r.h / 2;

    /* Center rectangle */
    hal_fb_fill_rect_blend(hal_rect(r.x + radius, r.y, r.w - 2 * radius, r.h), c);
    /* Left strip */
    hal_fb_fill_rect_blend(hal_rect(r.x, r.y + radius, radius, r.h - 2 * radius), c);
    /* Right strip */
    hal_fb_fill_rect_blend(hal_rect(r.x + r.w - radius, r.y + radius, radius, r.h - 2 * radius), c);

    /* Four corner quarter-circles with blending */
    int cx, cy;
    cx = r.x + radius; cy = r.y + radius;
    for (int py = 0; py < radius; py++)
        for (int px = 0; px < radius; px++)
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel_blend(cx - radius + px, cy - radius + py, c);

    cx = r.x + r.w - radius - 1; cy = r.y + radius;
    for (int py = 0; py < radius; py++)
        for (int px = 0; px < radius; px++)
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel_blend(cx + radius - px, cy - radius + py, c);

    cx = r.x + radius; cy = r.y + r.h - radius - 1;
    for (int py = 0; py < radius; py++)
        for (int px = 0; px < radius; px++)
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel_blend(cx - radius + px, cy + radius - py, c);

    cx = r.x + r.w - radius - 1; cy = r.y + r.h - radius - 1;
    for (int py = 0; py < radius; py++)
        for (int px = 0; px < radius; px++)
            if (px * px + py * py <= radius * radius)
                hal_fb_pixel_blend(cx + radius - px, cy + radius - py, c);
}

void hal_draw_rounded_rect(hal_rect_t r, int radius, hal_color_t c)
{
    if (radius <= 0) { hal_draw_rect(r, c); return; }
    /* Top/bottom edges */
    hal_fb_hline(r.x + radius, r.y, r.w - 2 * radius, c);
    hal_fb_hline(r.x + radius, r.y + r.h - 1, r.w - 2 * radius, c);
    /* Left/right edges */
    hal_fb_vline(r.x, r.y + radius, r.h - 2 * radius, c);
    hal_fb_vline(r.x + r.w - 1, r.y + radius, r.h - 2 * radius, c);

    /* Corner arcs */
    int cx, cy, px = 0, py = radius, d = 1 - radius;
    /* Top-left */
    cx = r.x + radius; cy = r.y + radius;
    px = 0; py = radius; d = 1 - radius;
    while (px <= py) {
        hal_fb_pixel_blend(cx - py, cy - px, c);
        hal_fb_pixel_blend(cx - px, cy - py, c);
        px++;
        if (d < 0) d += 2 * px + 1;
        else { py--; d += 2 * (px - py) + 1; }
    }
    /* Top-right */
    cx = r.x + r.w - 1 - radius; cy = r.y + radius;
    px = 0; py = radius; d = 1 - radius;
    while (px <= py) {
        hal_fb_pixel_blend(cx + py, cy - px, c);
        hal_fb_pixel_blend(cx + px, cy - py, c);
        px++;
        if (d < 0) d += 2 * px + 1;
        else { py--; d += 2 * (px - py) + 1; }
    }
    /* Bottom-left */
    cx = r.x + radius; cy = r.y + r.h - 1 - radius;
    px = 0; py = radius; d = 1 - radius;
    while (px <= py) {
        hal_fb_pixel_blend(cx - py, cy + px, c);
        hal_fb_pixel_blend(cx - px, cy + py, c);
        px++;
        if (d < 0) d += 2 * px + 1;
        else { py--; d += 2 * (px - py) + 1; }
    }
    /* Bottom-right */
    cx = r.x + r.w - 1 - radius; cy = r.y + r.h - 1 - radius;
    px = 0; py = radius; d = 1 - radius;
    while (px <= py) {
        hal_fb_pixel_blend(cx + py, cy + px, c);
        hal_fb_pixel_blend(cx + px, cy + py, c);
        px++;
        if (d < 0) d += 2 * px + 1;
        else { py--; d += 2 * (px - py) + 1; }
    }
}

/* ── Circle ──────────────────────────────────────────────────── */
void hal_draw_circle(int cx, int cy, int radius, hal_color_t c)
{
    int x = 0, y = radius, d = 1 - radius;
    while (x <= y) {
        hal_fb_pixel_blend(cx + x, cy + y, c);
        hal_fb_pixel_blend(cx - x, cy + y, c);
        hal_fb_pixel_blend(cx + x, cy - y, c);
        hal_fb_pixel_blend(cx - x, cy - y, c);
        hal_fb_pixel_blend(cx + y, cy + x, c);
        hal_fb_pixel_blend(cx - y, cy + x, c);
        hal_fb_pixel_blend(cx + y, cy - x, c);
        hal_fb_pixel_blend(cx - y, cy - x, c);
        x++;
        if (d < 0) d += 2 * x + 1;
        else { y--; d += 2 * (x - y) + 1; }
    }
}

void hal_draw_filled_circle(int cx, int cy, int radius, hal_color_t c)
{
    for (int y = -radius; y <= radius; y++) {
        int w = hal_isqrt(radius * radius - y * y);
        hal_fb_hline(cx - w, cy + y, 2 * w + 1, c);
    }
}

void hal_draw_filled_circle_blend(int cx, int cy, int radius, hal_color_t c)
{
    if (c.a == 255) { hal_draw_filled_circle(cx, cy, radius, c); return; }
    for (int y = -radius; y <= radius; y++) {
        int w = hal_isqrt(radius * radius - y * y);
        hal_fb_fill_rect_blend(hal_rect(cx - w, cy + y, 2 * w + 1, 1), c);
    }
}

/* ── Vertical gradient ───────────────────────────────────────── */
void hal_draw_gradient_v(hal_rect_t r, hal_color_t top, hal_color_t bottom)
{
    if (r.h <= 0) return;
    for (int y = 0; y < r.h; y++) {
        uint8_t cr = (uint8_t)(top.r + (bottom.r - top.r) * y / r.h);
        uint8_t cg = (uint8_t)(top.g + (bottom.g - top.g) * y / r.h);
        uint8_t cb = (uint8_t)(top.b + (bottom.b - top.b) * y / r.h);
        hal_fb_hline(r.x, r.y + y, r.w, hal_rgb(cr, cg, cb));
    }
}
