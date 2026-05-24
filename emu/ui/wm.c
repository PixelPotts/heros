#include "wm.h"
#include "draw.h"
#include "theme.h"
#include "icons.h"
#include "animation.h"
#include "../kernel/string.h"

static Window windows[MAX_WINDOWS];
static int    focus_id = -1;
static int    next_z = 1;

/* Drag state */
static int drag_win = -1;
static int drag_type = 0;   /* 1=move, 2=resize */
static int drag_ox, drag_oy;

void wm_init(void)
{
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].active = 0;
        windows[i].z_order = 0;
    }
    focus_id = -1;
    next_z = 1;
    drag_win = -1;
}

int wm_open(const char *title, int x, int y, int w, int h,
             uint32_t flags, int app_id, AppContent *content)
{
    int id = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].active) { id = i; break; }
    }
    if (id < 0) return -1;

    Window *win = &windows[id];
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';
    win->rect = RECT(x, y, w, h);
    win->restore_rect = win->rect;
    win->z_order = next_z++;
    win->flags = flags | WIN_VISIBLE;
    win->active = 1;
    win->app_id = app_id;
    win->content = content;

    wm_focus(id);

    /* Open animation */
    Rect from = RECT(x + w/4, y + h/4, w/2, h/2);
    anim_start(id, EASE_CUBIC_OUT, 200, from, win->rect, 0, 255);

    return id;
}

void wm_close(int win_id)
{
    if (win_id < 0 || win_id >= MAX_WINDOWS || !windows[win_id].active)
        return;

    Window *win = &windows[win_id];
    if (win->content) {
        if (win->content->on_close)
            win->content->on_close(win->content);
        if (win->content->destroy)
            win->content->destroy(win->content);
    }

    win->active = 0;
    win->content = (void *)0;

    if (focus_id == win_id) {
        /* Find next highest z-order window */
        focus_id = -1;
        int max_z = 0;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].active && !(windows[i].flags & WIN_MINIMIZED) &&
                windows[i].z_order > max_z) {
                max_z = windows[i].z_order;
                focus_id = i;
            }
        }
    }
}

void wm_focus(int win_id)
{
    if (win_id < 0 || win_id >= MAX_WINDOWS || !windows[win_id].active)
        return;
    focus_id = win_id;
    windows[win_id].z_order = next_z++;
    if (windows[win_id].flags & WIN_MINIMIZED)
        windows[win_id].flags &= ~WIN_MINIMIZED;
}

void wm_minimize(int win_id)
{
    if (win_id < 0 || win_id >= MAX_WINDOWS) return;
    windows[win_id].flags |= WIN_MINIMIZED;
    if (focus_id == win_id) {
        focus_id = -1;
        int max_z = 0;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].active && !(windows[i].flags & WIN_MINIMIZED) &&
                windows[i].z_order > max_z) {
                max_z = windows[i].z_order;
                focus_id = i;
            }
        }
    }
}

void wm_maximize(int win_id)
{
    if (win_id < 0 || win_id >= MAX_WINDOWS) return;
    Window *w = &windows[win_id];
    if (w->flags & WIN_MAXIMIZED) {
        wm_restore(win_id);
        return;
    }
    w->restore_rect = w->rect;
    w->rect = RECT(0, 28, SCREEN_W, SCREEN_H - 28 - 48);  /* between topbar and dock */
    w->flags |= WIN_MAXIMIZED;
}

void wm_restore(int win_id)
{
    if (win_id < 0 || win_id >= MAX_WINDOWS) return;
    Window *w = &windows[win_id];
    w->rect = w->restore_rect;
    w->flags &= ~WIN_MAXIMIZED;
}

/* ── Event handling ──────────────────────────────────────────── */

/* Get window at point, highest z-order first */
static int window_at(int x, int y)
{
    int best = -1, best_z = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].active) continue;
        if (windows[i].flags & WIN_MINIMIZED) continue;
        if (rect_contains(windows[i].rect, x, y)) {
            if (windows[i].z_order > best_z) {
                best_z = windows[i].z_order;
                best = i;
            }
        }
    }
    return best;
}

void wm_handle_mouse_down(int x, int y, int button)
{
    (void)button;
    int win_id = window_at(x, y);
    if (win_id < 0) return;

    wm_focus(win_id);
    Window *w = &windows[win_id];
    HitResult hit = window_hit_test(w, x, y);

    switch (hit) {
    case HIT_CLOSE_BTN:
        wm_close(win_id);
        break;
    case HIT_MIN_BTN:
        wm_minimize(win_id);
        break;
    case HIT_MAX_BTN:
        wm_maximize(win_id);
        break;
    case HIT_TITLE_BAR:
        if (!(w->flags & WIN_MAXIMIZED)) {
            drag_win = win_id;
            drag_type = 1;
            drag_ox = x - w->rect.x;
            drag_oy = y - w->rect.y;
        }
        break;
    case HIT_RESIZE_BR:
        if (!(w->flags & WIN_MAXIMIZED)) {
            drag_win = win_id;
            drag_type = 2;
            drag_ox = x;
            drag_oy = y;
        }
        break;
    case HIT_CONTENT:
        if (w->content && w->content->on_mouse_down) {
            Rect cr = window_content_rect(w);
            w->content->on_mouse_down(w->content, x - cr.x, y - cr.y);
        }
        break;
    default:
        break;
    }
}

void wm_handle_mouse_up(int x, int y, int button)
{
    (void)button;

    /* Snapping: if dragging near edge, snap */
    if (drag_win >= 0 && drag_type == 1) {
        Window *w = &windows[drag_win];
        if (y < 2) {
            /* Snap to top = maximize */
            wm_maximize(drag_win);
        } else if (x < 2) {
            /* Snap left half */
            w->restore_rect = w->rect;
            w->rect = RECT(0, 28, SCREEN_W / 2, SCREEN_H - 28 - 48);
        } else if (x >= SCREEN_W - 2) {
            /* Snap right half */
            w->restore_rect = w->rect;
            w->rect = RECT(SCREEN_W / 2, 28, SCREEN_W / 2, SCREEN_H - 28 - 48);
        }
    }

    if (drag_win >= 0 && focus_id >= 0 && focus_id < MAX_WINDOWS) {
        Window *fw = &windows[focus_id];
        if (fw->content && fw->content->on_mouse_up) {
            Rect cr = window_content_rect(fw);
            fw->content->on_mouse_up(fw->content, x - cr.x, y - cr.y);
        }
    }

    drag_win = -1;
    drag_type = 0;
}

void wm_handle_mouse_move(int x, int y)
{
    if (drag_win < 0 || drag_win >= MAX_WINDOWS) return;
    Window *w = &windows[drag_win];

    if (drag_type == 1) {
        /* Move */
        w->rect.x = x - drag_ox;
        w->rect.y = y - drag_oy;
        /* Clamp */
        if (w->rect.y < 0) w->rect.y = 0;
    } else if (drag_type == 2) {
        /* Resize */
        int dx = x - drag_ox;
        int dy = y - drag_oy;
        w->rect.w += dx;
        w->rect.h += dy;
        if (w->rect.w < MIN_WIN_W) w->rect.w = MIN_WIN_W;
        if (w->rect.h < MIN_WIN_H) w->rect.h = MIN_WIN_H;
        drag_ox = x;
        drag_oy = y;
        if (w->content && w->content->on_resize) {
            Rect cr = window_content_rect(w);
            w->content->on_resize(w->content, cr.w, cr.h);
        }
    }
}

void wm_handle_key_down(uint16_t key, uint16_t mod)
{
    if (focus_id >= 0 && focus_id < MAX_WINDOWS &&
        windows[focus_id].active && windows[focus_id].content) {
        if (windows[focus_id].content->on_key_down)
            windows[focus_id].content->on_key_down(
                windows[focus_id].content, key, mod);
    }
}

void wm_handle_text_input(char ch)
{
    if (focus_id >= 0 && focus_id < MAX_WINDOWS &&
        windows[focus_id].active && windows[focus_id].content) {
        if (windows[focus_id].content->on_text_input)
            windows[focus_id].content->on_text_input(
                windows[focus_id].content, ch);
    }
}

void wm_handle_scroll(int x, int y, int scroll_y)
{
    int win_id = window_at(x, y);
    if (win_id >= 0 && windows[win_id].content &&
        windows[win_id].content->on_scroll) {
        Rect cr = window_content_rect(&windows[win_id]);
        windows[win_id].content->on_scroll(
            windows[win_id].content, x - cr.x, y - cr.y, scroll_y);
    }
}

/* ── Rendering ───────────────────────────────────────────────── */

/* Sort windows by z-order for rendering (lowest first) */
static int z_sorted[MAX_WINDOWS];

static void sort_by_z(void)
{
    int count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].active && !(windows[i].flags & WIN_MINIMIZED))
            z_sorted[count++] = i;
    }
    /* Simple bubble sort (max 16 windows) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (windows[z_sorted[j]].z_order > windows[z_sorted[j+1]].z_order) {
                int tmp = z_sorted[j];
                z_sorted[j] = z_sorted[j+1];
                z_sorted[j+1] = tmp;
            }
        }
    }
    /* Sentinel */
    for (int i = count; i < MAX_WINDOWS; i++)
        z_sorted[i] = -1;
}

static void render_window(int win_id)
{
    Window *w = &windows[win_id];
    const ThemeColors *tc = theme_colors();
    int focused = (win_id == focus_id);

    /* Check for animation */
    Rect draw_rect = w->rect;
    int alpha = 255;
    anim_tick(win_id, &draw_rect, &alpha);

    /* Shadow */
    Color shadow = tc->window_shadow;
    shadow.a = (uint8_t)(shadow.a * alpha / 255);
    draw_filled_rounded_rect_blend(
        RECT(draw_rect.x + 3, draw_rect.y + 3, draw_rect.w, draw_rect.h),
        6, shadow);

    /* Window background */
    Color bg = tc->window_bg;
    bg.a = (uint8_t)(bg.a * alpha / 255);
    draw_filled_rounded_rect_blend(draw_rect, 6, bg);

    /* Title bar */
    Color title_bg = focused ? tc->window_title_bg : tc->panel_bg;
    title_bg.a = (uint8_t)(title_bg.a * alpha / 255);
    draw_filled_rounded_rect_blend(
        RECT(draw_rect.x, draw_rect.y, draw_rect.w, TITLE_BAR_H),
        6, title_bg);
    /* Fix bottom corners of title bar */
    draw_filled_rect_blend(
        RECT(draw_rect.x, draw_rect.y + TITLE_BAR_H - 6, draw_rect.w, 6),
        title_bg);

    /* Title text */
    Color title_c = focused ? tc->text_primary : tc->text_secondary;
    draw_text(draw_rect.x + 10, draw_rect.y + 8, w->title,
              title_c, FONT_SIZE_SMALL);

    /* Window control buttons */
    int btn_y = draw_rect.y + 4;
    int btn_size = 20;
    int bx = draw_rect.x + draw_rect.w - btn_size - 6;

    /* Close button (red) */
    draw_filled_circle(bx + btn_size/2, btn_y + btn_size/2, 6,
                       COLOR(200, 60, 60, 200));
    icon_draw(ICON_CLOSE, bx + 3, btn_y + 3, btn_size - 6, COLOR_WHITE);

    /* Maximize button */
    bx -= btn_size + 4;
    draw_filled_circle(bx + btn_size/2, btn_y + btn_size/2, 6,
                       COLOR(60, 180, 60, 200));

    /* Minimize button */
    bx -= btn_size + 4;
    draw_filled_circle(bx + btn_size/2, btn_y + btn_size/2, 6,
                       COLOR(230, 180, 40, 200));

    /* Border */
    Color border = focused ? tc->accent_dim : tc->window_border;
    draw_rounded_rect(draw_rect, 6, border);

    /* Content */
    if (w->content && w->content->render) {
        Rect cr = RECT(draw_rect.x + WIN_BORDER,
                        draw_rect.y + TITLE_BAR_H,
                        draw_rect.w - 2 * WIN_BORDER,
                        draw_rect.h - TITLE_BAR_H - WIN_BORDER);
        w->content->render(w->content, cr);
    }
}

void wm_render(void)
{
    sort_by_z();
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (z_sorted[i] < 0) break;
        render_window(z_sorted[i]);
    }
}

void wm_tick(void)
{
    /* Nothing needed for now — animations handled during render */
}

int wm_window_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].active) count++;
    return count;
}

Window *wm_get_window(int win_id)
{
    if (win_id < 0 || win_id >= MAX_WINDOWS || !windows[win_id].active)
        return (void *)0;
    return &windows[win_id];
}

int wm_find_by_app(int app_id)
{
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].active && windows[i].app_id == app_id)
            return i;
    }
    return -1;
}
