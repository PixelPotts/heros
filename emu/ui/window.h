#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "types.h"

#define MAX_WINDOWS     16
#define TITLE_BAR_H     28
#define MIN_WIN_W       200
#define MIN_WIN_H       150
#define WIN_BORDER      1

/* Window flags */
#define WIN_VISIBLE     (1 << 0)
#define WIN_MINIMIZED   (1 << 1)
#define WIN_MAXIMIZED   (1 << 2)
#define WIN_CLOSABLE    (1 << 3)
#define WIN_RESIZABLE   (1 << 4)

/* Forward declare */
struct AppContent;

/* AppContent — virtual dispatch via function pointers */
typedef struct AppContent {
    void (*render)(struct AppContent *self, Rect content_rect);
    void (*on_mouse_down)(struct AppContent *self, int x, int y);
    void (*on_mouse_up)(struct AppContent *self, int x, int y);
    void (*on_key_down)(struct AppContent *self, uint16_t key, uint16_t mod);
    void (*on_text_input)(struct AppContent *self, char ch);
    void (*on_scroll)(struct AppContent *self, int x, int y, int scroll_y);
    void (*on_resize)(struct AppContent *self, int w, int h);
    int  (*on_close)(struct AppContent *self);
    void (*destroy)(struct AppContent *self);
    void *data;
} AppContent;

typedef struct {
    char        title[64];
    Rect        rect;              /* position + size including title bar */
    Rect        restore_rect;      /* for restore from maximize */
    int         z_order;
    uint32_t    flags;
    int         active;            /* whether this window slot is used */
    int         app_id;            /* which app this belongs to */
    AppContent *content;
} Window;

/* Hit test result */
typedef enum {
    HIT_NONE = 0,
    HIT_TITLE_BAR,
    HIT_CLOSE_BTN,
    HIT_MIN_BTN,
    HIT_MAX_BTN,
    HIT_CONTENT,
    HIT_RESIZE_BR,   /* bottom-right corner resize handle */
} HitResult;

/* Get content rect (below title bar) */
Rect window_content_rect(const Window *w);

/* Hit testing */
HitResult window_hit_test(const Window *w, int x, int y);

#endif
