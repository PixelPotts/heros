#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../icons.h"
#include "../widgets.h"
#include "../window.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"
#include "../../hal/hal_fs.h"

#define FM_MAX_ENTRIES  64

typedef struct {
    fs_dirent_t entries[FM_MAX_ENTRIES];
    int         entry_count;
    int         selected;
    int         scroll;
    char        cwd[256];
} FMData;

static void fm_refresh(FMData *fm)
{
    fm->entry_count = 0;
    fm->selected = -1;
    fm->scroll = 0;

    int fd = hal_fs_open(fm->cwd, FS_O_READ);
    if (fd < 0) return;

    fs_dirent_t entry;
    while (fm->entry_count < FM_MAX_ENTRIES &&
           hal_fs_readdir(fd, &entry) > 0) {
        fm->entries[fm->entry_count++] = entry;
    }
    hal_fs_close(fd);
}

static void fm_render(AppContent *self, Rect cr)
{
    FMData *fm = (FMData *)self->data;
    const ThemeColors *tc = theme_colors();

    /* Header with path */
    draw_filled_rect_blend(RECT(cr.x, cr.y, cr.w, 24), tc->panel_bg);
    draw_text(cr.x + 8, cr.y + 6, fm->cwd, tc->text_primary, FONT_SIZE_SMALL);
    draw_line(cr.x, cr.y + 23, cr.x + cr.w, cr.y + 23, tc->panel_border);

    /* Column headers */
    int y = cr.y + 28;
    draw_text(cr.x + 36, y, "Name", tc->text_muted, FONT_SIZE_SMALL);
    draw_text(cr.x + cr.w - 80, y, "Size", tc->text_muted, FONT_SIZE_SMALL);
    y += 16;
    widget_separator(cr.x, y, cr.w);
    y += 4;

    /* File list */
    int row_h = 22;
    int visible_rows = (cr.h - 50) / row_h;
    int mx, my;
    hal_input_mouse_pos(&mx, &my);

    for (int i = fm->scroll; i < fm->entry_count && i < fm->scroll + visible_rows; i++) {
        Rect row = RECT(cr.x, y, cr.w, row_h);
        int sel = (i == fm->selected);

        widget_list_item(row, "", (void *)0, sel, mx, my);

        /* Icon */
        IconId ic = (fm->entries[i].type == FS_TYPE_DIR) ? ICON_FOLDER : ICON_FILE;
        Color icon_c = (fm->entries[i].type == FS_TYPE_DIR) ? tc->accent : tc->text_secondary;
        icon_draw(ic, cr.x + 8, y + 2, 16, icon_c);

        /* Name */
        draw_text(cr.x + 30, y + 5, fm->entries[i].name,
                  sel ? COLOR_WHITE : tc->text_primary, FONT_SIZE_SMALL);

        /* Size (files only) */
        if (fm->entries[i].type == FS_TYPE_FILE) {
            char sz[16];
            utoa(fm->entries[i].size, sz, 10);
            draw_text_right(cr.x + cr.w - 8, y + 5, sz,
                           tc->text_muted, FONT_SIZE_SMALL);
        } else {
            draw_text_right(cr.x + cr.w - 8, y + 5, "DIR",
                           tc->text_muted, FONT_SIZE_SMALL);
        }

        y += row_h;
    }

    /* Status bar */
    y = cr.y + cr.h - 18;
    draw_filled_rect_blend(RECT(cr.x, y, cr.w, 18), tc->panel_bg);
    char status[64];
    strcpy(status, "");
    char cnt[8];
    itoa(fm->entry_count, cnt, 10);
    strcat(status, cnt);
    strcat(status, " items");
    draw_text(cr.x + 8, y + 4, status, tc->text_muted, FONT_SIZE_SMALL);
}

static void fm_on_mouse_down(AppContent *self, int x, int y)
{
    FMData *fm = (FMData *)self->data;
    /* Calculate which row was clicked */
    int row_h = 22;
    int header_h = 50;
    if (y < header_h) return;

    int idx = fm->scroll + (y - header_h) / row_h;
    if (idx >= 0 && idx < fm->entry_count) {
        fm->selected = idx;
    }
    (void)x;
}

static void fm_on_scroll(AppContent *self, int x, int y, int sy)
{
    FMData *fm = (FMData *)self->data;
    (void)x; (void)y;
    fm->scroll -= sy * 3;
    if (fm->scroll < 0) fm->scroll = 0;
    if (fm->scroll >= fm->entry_count) fm->scroll = fm->entry_count - 1;
    if (fm->scroll < 0) fm->scroll = 0;
}

static void fm_destroy(AppContent *self)
{
    if (self->data) kfree(self->data);
    kfree(self);
}

AppContent *filemanager_create(void)
{
    AppContent *app = (AppContent *)kmalloc(sizeof(AppContent));
    if (!app) return (void *)0;
    memset(app, 0, sizeof(AppContent));

    FMData *fm = (FMData *)kmalloc(sizeof(FMData));
    if (!fm) { kfree(app); return (void *)0; }
    memset(fm, 0, sizeof(FMData));

    strcpy(fm->cwd, "/");
    fm_refresh(fm);

    app->render = fm_render;
    app->on_mouse_down = fm_on_mouse_down;
    app->on_scroll = fm_on_scroll;
    app->destroy = fm_destroy;
    app->data = fm;

    return app;
}
