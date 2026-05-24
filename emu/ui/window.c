#include "window.h"

Rect window_content_rect(const Window *w)
{
    return RECT(w->rect.x + WIN_BORDER,
                w->rect.y + TITLE_BAR_H,
                w->rect.w - 2 * WIN_BORDER,
                w->rect.h - TITLE_BAR_H - WIN_BORDER);
}

HitResult window_hit_test(const Window *w, int x, int y)
{
    if (!rect_contains(w->rect, x, y))
        return HIT_NONE;

    /* Check title bar buttons (right side) */
    int btn_y = w->rect.y + 4;
    int btn_size = 20;

    /* Close button */
    Rect close_r = RECT(w->rect.x + w->rect.w - btn_size - 6, btn_y,
                         btn_size, btn_size);
    if (rect_contains(close_r, x, y))
        return HIT_CLOSE_BTN;

    /* Maximize button */
    Rect max_r = RECT(close_r.x - btn_size - 4, btn_y, btn_size, btn_size);
    if (rect_contains(max_r, x, y))
        return HIT_MAX_BTN;

    /* Minimize button */
    Rect min_r = RECT(max_r.x - btn_size - 4, btn_y, btn_size, btn_size);
    if (rect_contains(min_r, x, y))
        return HIT_MIN_BTN;

    /* Title bar (drag area) */
    if (y < w->rect.y + TITLE_BAR_H)
        return HIT_TITLE_BAR;

    /* Resize handle (bottom-right 12x12 corner) */
    if ((w->flags & WIN_RESIZABLE) &&
        x > w->rect.x + w->rect.w - 12 &&
        y > w->rect.y + w->rect.h - 12)
        return HIT_RESIZE_BR;

    return HIT_CONTENT;
}
