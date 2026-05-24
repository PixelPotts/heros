#ifndef UI_DRAW_H
#define UI_DRAW_H

#include "types.h"

/* Thin wrappers mapping old draw:: style to HAL */
void draw_rect(Rect r, Color c);
void draw_filled_rect(Rect r, Color c);
void draw_filled_rect_blend(Rect r, Color c);
void draw_rounded_rect(Rect r, int radius, Color c);
void draw_filled_rounded_rect(Rect r, int radius, Color c);
void draw_filled_rounded_rect_blend(Rect r, int radius, Color c);
void draw_line(int x0, int y0, int x1, int y1, Color c);
void draw_circle(int cx, int cy, int r, Color c);
void draw_filled_circle(int cx, int cy, int r, Color c);
void draw_filled_circle_blend(int cx, int cy, int r, Color c);
void draw_gradient_v(Rect r, Color top, Color bottom);
void draw_text(int x, int y, const char *text, Color c, hal_font_size_t size);
void draw_text_centered(int cx, int y, const char *text, Color c, hal_font_size_t size);
void draw_text_right(int rx, int y, const char *text, Color c, hal_font_size_t size);
int  draw_text_width(const char *text, hal_font_size_t size);

#endif
