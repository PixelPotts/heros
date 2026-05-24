#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../widgets.h"
#include "../window.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"
#include "../../kernel/sched.h"
#include "../../kernel/timer.h"

typedef struct {
    int selected;
} TMData;

static void tm_render(AppContent *self, Rect cr)
{
    TMData *tm = (TMData *)self->data;
    const ThemeColors *tc = theme_colors();
    int mx, my;
    hal_input_mouse_pos(&mx, &my);

    int y = cr.y + 8;

    /* Title */
    draw_text(cr.x + 12, y, "Task Manager", tc->text_primary, FONT_SIZE_MEDIUM);
    y += 24;

    /* Memory overview */
    widget_card(RECT(cr.x + 8, y, cr.w - 16, 50), 6);
    {
        size_t total = mm_total_pages();
        size_t free_p = mm_free_pages();
        size_t used = total - free_p;

        draw_text(cr.x + 16, y + 6, "Memory Usage", tc->text_muted, FONT_SIZE_SMALL);

        char mem_str[48];
        strcpy(mem_str, "");
        char buf[16];
        utoa((unsigned)(used * 4), buf, 10);
        strcat(mem_str, buf);
        strcat(mem_str, " / ");
        utoa((unsigned)(total * 4), buf, 10);
        strcat(mem_str, buf);
        strcat(mem_str, " KB");
        draw_text(cr.x + 16, y + 20, mem_str, tc->text_primary, FONT_SIZE_SMALL);

        widget_progress(RECT(cr.x + 16, y + 36, cr.w - 32, 6),
                        (int)used, (int)total);
    }
    y += 58;

    /* Uptime */
    char up_str[48];
    uint32_t ms = hal_get_ticks();
    strcpy(up_str, "Uptime: ");
    char buf[16];
    utoa(ms / 1000, buf, 10);
    strcat(up_str, buf);
    strcat(up_str, "s");
    draw_text(cr.x + 12, y, up_str, tc->text_muted, FONT_SIZE_SMALL);
    y += 18;

    /* Task list header */
    widget_separator(cr.x + 8, y, cr.w - 16);
    y += 6;

    draw_text(cr.x + 12, y, "PID", tc->text_muted, FONT_SIZE_SMALL);
    draw_text(cr.x + 42, y, "Name", tc->text_muted, FONT_SIZE_SMALL);
    draw_text_right(cr.x + cr.w - 12, y, "State", tc->text_muted, FONT_SIZE_SMALL);
    y += 14;

    /* Task list */
    for (int i = 0; i < MAX_TASKS; i++) {
        task_state_t state = sched_task_state(i);
        if (state == TASK_FREE) continue;

        Rect row = RECT(cr.x + 8, y, cr.w - 16, 22);
        int sel = (i == tm->selected);

        /* State color */
        Color state_c;
        const char *state_str;
        switch (state) {
        case TASK_RUNNING:
            state_c = tc->success;
            state_str = "Running";
            break;
        case TASK_READY:
            state_c = tc->accent;
            state_str = "Ready";
            break;
        case TASK_SLEEPING:
            state_c = tc->warning;
            state_str = "Sleep";
            break;
        case TASK_BLOCKED:
            state_c = tc->error;
            state_str = "Blocked";
            break;
        case TASK_DEAD:
            state_c = tc->text_muted;
            state_str = "Dead";
            break;
        default:
            state_c = tc->text_muted;
            state_str = "???";
        }

        if (sel)
            draw_filled_rounded_rect_blend(row, 4, tc->accent_dim);
        else if (rect_contains(row, mx, my))
            draw_filled_rounded_rect_blend(row, 4, COLOR(255, 255, 255, 10));

        char pid[8];
        itoa(i, pid, 10);
        draw_text(cr.x + 16, y + 5, pid,
                  sel ? COLOR_WHITE : tc->text_primary, FONT_SIZE_SMALL);

        draw_text(cr.x + 42, y + 5, sched_task_name(i),
                  sel ? COLOR_WHITE : tc->text_primary, FONT_SIZE_SMALL);

        widget_status_dot(cr.x + cr.w - 70, y + 11, 3, state_c);
        draw_text_right(cr.x + cr.w - 16, y + 5, state_str,
                       state_c, FONT_SIZE_SMALL);

        y += 24;
        if (y + 24 > cr.y + cr.h) break;
    }
}

static void tm_on_mouse_down(AppContent *self, int x, int y)
{
    TMData *tm = (TMData *)self->data;
    (void)x;

    /* Calculate which task was clicked */
    int start_y = 8 + 24 + 58 + 18 + 6 + 14;
    int idx = (y - start_y) / 24;

    /* Map visible row to actual task id */
    int visible = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (sched_task_state(i) == TASK_FREE) continue;
        if (visible == idx) {
            tm->selected = i;
            return;
        }
        visible++;
    }
}

static void tm_destroy(AppContent *self)
{
    if (self->data) kfree(self->data);
    kfree(self);
}

AppContent *taskmanager_create(void)
{
    AppContent *app = (AppContent *)kmalloc(sizeof(AppContent));
    if (!app) return (void *)0;
    memset(app, 0, sizeof(AppContent));

    TMData *tm = (TMData *)kmalloc(sizeof(TMData));
    if (!tm) { kfree(app); return (void *)0; }
    memset(tm, 0, sizeof(TMData));
    tm->selected = -1;

    app->render = tm_render;
    app->on_mouse_down = tm_on_mouse_down;
    app->destroy = tm_destroy;
    app->data = tm;

    return app;
}
