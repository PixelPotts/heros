#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "types.h"

/* Button styles */
typedef enum {
    BTN_PRIMARY,
    BTN_SECONDARY,
    BTN_GHOST,
    BTN_DANGER
} ButtonStyle;

/* Draw a card (rounded rect with bg + border) */
void widget_card(Rect r, int radius);

/* Draw a button — returns 1 if mouse is hovering */
int  widget_button(Rect r, const char *label, ButtonStyle style,
                    int mx, int my, int pressed);

/* Toggle switch — returns visual state */
void widget_toggle(int x, int y, int on);

/* Progress bar */
void widget_progress(Rect r, int value, int max_val);

/* Badge (small pill with text) */
void widget_badge(int x, int y, const char *text, Color bg);

/* Separator line */
void widget_separator(int x, int y, int w);

/* Section header text */
void widget_section_header(int x, int y, const char *text);

/* Status dot (green/yellow/red) */
void widget_status_dot(int x, int y, int radius, Color c);

/* List item (text with optional right-side text) */
void widget_list_item(Rect r, const char *text, const char *right_text,
                       int selected, int mx, int my);

#endif
