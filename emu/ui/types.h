#ifndef UI_TYPES_H
#define UI_TYPES_H

#include "../hal/hal.h"
#include "../hal/hal_fb.h"
#include "../hal/hal_draw.h"
#include "../hal/hal_text.h"
#include "../hal/hal_input.h"
#include "../hal/hal_time.h"
#include "../hal/hal_mem.h"
#include "../hal/hal_fs.h"

/* ── Color typedef ─────────────────────────────────────────────── */
typedef hal_color_t Color;
typedef hal_rect_t  Rect;
typedef hal_point_t Point;

/* Convenience macros */
#define COLOR(r,g,b,a)  hal_color(r,g,b,a)
#define RGB(r,g,b)      hal_rgb(r,g,b)
#define RECT(x,y,w,h)   hal_rect(x,y,w,h)

/* Screen dimensions */
#define SCREEN_W  HAL_SCREEN_W
#define SCREEN_H  HAL_SCREEN_H

/* Commonly used colors */
#define COLOR_WHITE       RGB(255, 255, 255)
#define COLOR_BLACK       RGB(0, 0, 0)
#define COLOR_TRANSPARENT COLOR(0, 0, 0, 0)

static inline int rect_contains(Rect r, int x, int y)
{
    return hal_rect_contains(r, x, y);
}

static inline int min_i(int a, int b) { return a < b ? a : b; }
static inline int max_i(int a, int b) { return a > b ? a : b; }
static inline int clamp_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

#endif
