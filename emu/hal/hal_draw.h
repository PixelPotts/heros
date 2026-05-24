#ifndef HAL_DRAW_H
#define HAL_DRAW_H

#include "hal.h"

/* Primitives */
void hal_draw_line(int x0, int y0, int x1, int y1, hal_color_t c);
void hal_draw_rect(hal_rect_t r, hal_color_t c);
void hal_draw_filled_rect(hal_rect_t r, hal_color_t c);
void hal_draw_filled_rect_blend(hal_rect_t r, hal_color_t c);

/* Rounded rectangles */
void hal_draw_rounded_rect(hal_rect_t r, int radius, hal_color_t c);
void hal_draw_filled_rounded_rect(hal_rect_t r, int radius, hal_color_t c);
void hal_draw_filled_rounded_rect_blend(hal_rect_t r, int radius, hal_color_t c);

/* Circles */
void hal_draw_circle(int cx, int cy, int radius, hal_color_t c);
void hal_draw_filled_circle(int cx, int cy, int radius, hal_color_t c);
void hal_draw_filled_circle_blend(int cx, int cy, int radius, hal_color_t c);

/* Special effects */
void hal_draw_gradient_v(hal_rect_t r, hal_color_t top, hal_color_t bottom);

/* Trig helpers */
int hal_sin256(int angle);  /* sin(angle*2pi/256) * 256 */
int hal_cos256(int angle);  /* cos(angle*2pi/256) * 256 */
unsigned int hal_isqrt(unsigned int n);

#endif
