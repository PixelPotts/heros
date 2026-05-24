#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

/* ── Core types ────────────────────────────────────────────────── */

typedef struct {
    uint8_t r, g, b, a;
} hal_color_t;

typedef struct {
    int x, y, w, h;
} hal_rect_t;

typedef struct {
    int x, y;
} hal_point_t;

/* ── Color constructors ───────────────────────────────────────── */

static inline hal_color_t hal_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    hal_color_t c = {r, g, b, a};
    return c;
}

static inline hal_color_t hal_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    hal_color_t c = {r, g, b, 255};
    return c;
}

static inline hal_color_t hal_rgba_from_hex(uint32_t hex)
{
    hal_color_t c;
    c.r = (hex >> 24) & 0xFF;
    c.g = (hex >> 16) & 0xFF;
    c.b = (hex >> 8) & 0xFF;
    c.a = hex & 0xFF;
    return c;
}

/* ── Rect helpers ─────────────────────────────────────────────── */

static inline int hal_rect_contains(hal_rect_t r, int px, int py)
{
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

static inline hal_rect_t hal_rect(int x, int y, int w, int h)
{
    hal_rect_t r = {x, y, w, h};
    return r;
}

#endif
