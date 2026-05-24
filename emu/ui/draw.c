#include "draw.h"

void draw_rect(Rect r, Color c) { hal_draw_rect(r, c); }
void draw_filled_rect(Rect r, Color c) { hal_fb_fill_rect(r, c); }
void draw_filled_rect_blend(Rect r, Color c) { hal_fb_fill_rect_blend(r, c); }
void draw_rounded_rect(Rect r, int radius, Color c) { hal_draw_rounded_rect(r, radius, c); }
void draw_filled_rounded_rect(Rect r, int radius, Color c) { hal_draw_filled_rounded_rect(r, radius, c); }
void draw_filled_rounded_rect_blend(Rect r, int radius, Color c) { hal_draw_filled_rounded_rect_blend(r, radius, c); }
void draw_line(int x0, int y0, int x1, int y1, Color c) { hal_draw_line(x0, y0, x1, y1, c); }
void draw_circle(int cx, int cy, int r, Color c) { hal_draw_circle(cx, cy, r, c); }
void draw_filled_circle(int cx, int cy, int r, Color c) { hal_draw_filled_circle(cx, cy, r, c); }
void draw_filled_circle_blend(int cx, int cy, int r, Color c) { hal_draw_filled_circle_blend(cx, cy, r, c); }
void draw_gradient_v(Rect r, Color top, Color bottom) { hal_draw_gradient_v(r, top, bottom); }
void draw_text(int x, int y, const char *text, Color c, hal_font_size_t size) { hal_text(x, y, text, c, size); }
void draw_text_centered(int cx, int y, const char *text, Color c, hal_font_size_t size) { hal_text_centered(cx, y, text, c, size); }
void draw_text_right(int rx, int y, const char *text, Color c, hal_font_size_t size) { hal_text_right(rx, y, text, c, size); }
int draw_text_width(const char *text, hal_font_size_t size) { return hal_text_width(text, size); }
