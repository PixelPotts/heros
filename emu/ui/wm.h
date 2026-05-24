#ifndef UI_WM_H
#define UI_WM_H

#include "window.h"

void    wm_init(void);
int     wm_open(const char *title, int x, int y, int w, int h,
                 uint32_t flags, int app_id, AppContent *content);
void    wm_close(int win_id);
void    wm_focus(int win_id);
void    wm_minimize(int win_id);
void    wm_maximize(int win_id);
void    wm_restore(int win_id);

/* Event handling */
void    wm_handle_mouse_down(int x, int y, int button);
void    wm_handle_mouse_up(int x, int y, int button);
void    wm_handle_mouse_move(int x, int y);
void    wm_handle_key_down(uint16_t key, uint16_t mod);
void    wm_handle_text_input(char ch);
void    wm_handle_scroll(int x, int y, int scroll_y);

/* Rendering */
void    wm_render(void);
void    wm_tick(void);

/* Query */
int     wm_window_count(void);
Window *wm_get_window(int win_id);
int     wm_find_by_app(int app_id);
int     wm_last_mouse_button(void);

#endif
