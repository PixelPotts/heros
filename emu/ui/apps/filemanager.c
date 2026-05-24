#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../icons.h"
#include "../widgets.h"
#include "../window.h"
#include "../wm.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"
#include "../../hal/hal_fs.h"

/*──────────────────────────────────────────────────────────────────
 * Layout constants
 *────────────────────────────────────────────────────────────────*/
#define FM_MAX_ENTRIES  128
#define FM_MAX_HIST     16
#define TOOLBAR_H       26
#define SIDEBAR_W       100
#define COLHDR_H        16
#define ROW_H           20
#define STATUSBAR_H     18
#define CTX_W           140
#define CTX_ITEM_H      20
#define CTX_SEP_H       8
#define DBLCLICK_MS     400

/* Sort columns */
#define SORT_NAME  0
#define SORT_SIZE  1
#define SORT_TYPE  2

/* Dialog types */
#define DLG_NONE     0
#define DLG_NEWFILE  1
#define DLG_RENAME   2
#define DLG_DELETE   3
#define DLG_MSG      4

/* Context menu actions */
#define ACT_OPEN     0
#define ACT_NEWFILE  1
#define ACT_RENAME   2
#define ACT_DELETE   3
#define ACT_REFRESH  4
#define ACT_HIDDEN   5

/*──────────────────────────────────────────────────────────────────
 * Types
 *────────────────────────────────────────────────────────────────*/
typedef struct {
    const char *label;
    int  action;
    int  enabled;
    int  sep_above;
} CtxItem;

typedef struct {
    /* Directory contents */
    fs_dirent_t entries[FM_MAX_ENTRIES];
    int         entry_count;
    char        cwd[256];

    /* Sorted/filtered view — indices into entries[] */
    int         view[FM_MAX_ENTRIES];
    int         view_count;

    /* Navigation history */
    char        history[FM_MAX_HIST][256];
    int         hist_n, hist_i;

    /* Selection & scroll */
    int         selected;       /* index in view[], -1 = none */
    int         scroll;

    /* Sort / filter */
    int         sort_col;
    int         sort_asc;
    int         show_hidden;

    /* Context menu */
    int         ctx_vis;
    int         ctx_x, ctx_y;   /* content-relative */
    int         ctx_target;     /* view index, -1 = background */
    CtxItem     ctx_items[8];
    int         ctx_n;

    /* Dialog */
    int         dlg;
    char        dlg_buf[64];
    int         dlg_cur;
    char        dlg_title[48];
    char        dlg_msg[96];

    /* Double-click detection */
    uint32_t    dbl_time;
    int         dbl_item;

    /* Type-ahead filter */
    char        filter[32];
    int         filter_len;

    /* Cached content dimensions */
    int         cw, ch;
} FMData;

/*──────────────────────────────────────────────────────────────────
 * Helpers
 *────────────────────────────────────────────────────────────────*/
static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int str_match_i(const char *s, const char *q, int qlen)
{
    if (qlen <= 0) return 1;
    int slen = (int)strlen(s);
    for (int i = 0; i <= slen - qlen; i++) {
        int ok = 1;
        for (int j = 0; j < qlen && ok; j++)
            if (to_lower(s[i + j]) != to_lower(q[j])) ok = 0;
        if (ok) return 1;
    }
    return 0;
}

static void fmt_size(uint32_t b, char *out)
{
    if (b < 1024) {
        utoa(b, out, 10);
        strcat(out, " B");
    } else if (b < 1024 * 1024) {
        utoa(b / 1024, out, 10);
        strcat(out, " KB");
    } else {
        utoa(b / (1024 * 1024), out, 10);
        strcat(out, " MB");
    }
}

static const char *ftype_str(const fs_dirent_t *e)
{
    if (e->type == FS_TYPE_DIR) return "Folder";
    const char *dot = strrchr(e->name, '.');
    if (!dot) return "File";
    if (strcmp(dot, ".TXT") == 0 || strcmp(dot, ".txt") == 0) return "Text";
    if (strcmp(dot, ".C") == 0   || strcmp(dot, ".c") == 0)   return "C Source";
    if (strcmp(dot, ".H") == 0   || strcmp(dot, ".h") == 0)   return "Header";
    if (strcmp(dot, ".S") == 0   || strcmp(dot, ".s") == 0)   return "Assembly";
    if (strcmp(dot, ".SH") == 0  || strcmp(dot, ".sh") == 0)  return "Script";
    if (strcmp(dot, ".MD") == 0  || strcmp(dot, ".md") == 0)  return "Markdown";
    if (strcmp(dot, ".BIN") == 0 || strcmp(dot, ".bin") == 0) return "Binary";
    if (strcmp(dot, ".IMG") == 0 || strcmp(dot, ".img") == 0) return "Disk Img";
    if (strcmp(dot, ".CFG") == 0 || strcmp(dot, ".cfg") == 0) return "Config";
    return "File";
}

static void build_path(const char *cwd, const char *name, char *out)
{
    if (strcmp(cwd, "/") == 0) {
        strcpy(out, "/");
        strcat(out, name);
    } else {
        strncpy(out, cwd, 200);
        strcat(out, "/");
        strcat(out, name);
    }
}

/*──────────────────────────────────────────────────────────────────
 * View rebuild (filter + sort)
 *────────────────────────────────────────────────────────────────*/
static int entry_cmp(const FMData *fm, int a, int b)
{
    const fs_dirent_t *ea = &fm->entries[a];
    const fs_dirent_t *eb = &fm->entries[b];

    /* ".." always first */
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return  1;

    /* Directories before files */
    if (ea->type == FS_TYPE_DIR && eb->type != FS_TYPE_DIR) return -1;
    if (ea->type != FS_TYPE_DIR && eb->type == FS_TYPE_DIR) return  1;

    int c = 0;
    switch (fm->sort_col) {
    case SORT_NAME:
        c = strcmp(ea->name, eb->name);
        break;
    case SORT_SIZE:
        c = (ea->size > eb->size) ? 1 : (ea->size < eb->size) ? -1 : 0;
        break;
    case SORT_TYPE:
        c = strcmp(ftype_str(ea), ftype_str(eb));
        break;
    }
    return fm->sort_asc ? c : -c;
}

static void rebuild_view(FMData *fm)
{
    fm->view_count = 0;
    for (int i = 0; i < fm->entry_count; i++) {
        const char *name = fm->entries[i].name;
        if (strcmp(name, ".") == 0) continue;
        if (!fm->show_hidden && name[0] == '.' && strcmp(name, "..") != 0)
            continue;
        if (!str_match_i(name, fm->filter, fm->filter_len))
            continue;
        fm->view[fm->view_count++] = i;
    }
    /* Insertion sort */
    for (int i = 1; i < fm->view_count; i++) {
        int k = fm->view[i];
        int j = i - 1;
        while (j >= 0 && entry_cmp(fm, fm->view[j], k) > 0) {
            fm->view[j + 1] = fm->view[j];
            j--;
        }
        fm->view[j + 1] = k;
    }
}

/*──────────────────────────────────────────────────────────────────
 * Directory operations
 *────────────────────────────────────────────────────────────────*/
static void fm_load(FMData *fm)
{
    fm->entry_count = 0;
    fm->selected = -1;
    fm->scroll = 0;

    int fd = hal_fs_open(fm->cwd, FS_O_READ);
    if (fd < 0) return;

    fs_dirent_t e;
    while (fm->entry_count < FM_MAX_ENTRIES && hal_fs_readdir(fd, &e) > 0)
        fm->entries[fm->entry_count++] = e;
    hal_fs_close(fd);
    rebuild_view(fm);
}

static void fm_navigate(FMData *fm, const char *path)
{
    /* Push to history (truncate forward history) */
    if (fm->hist_i < FM_MAX_HIST - 1)
        fm->hist_i++;
    strncpy(fm->history[fm->hist_i], path, 255);
    fm->history[fm->hist_i][255] = '\0';
    fm->hist_n = fm->hist_i + 1;

    strncpy(fm->cwd, path, 255);
    fm->cwd[255] = '\0';
    fm->filter_len = 0;
    fm->filter[0] = '\0';
    fm_load(fm);
}

static void fm_open_selected(FMData *fm)
{
    if (fm->selected < 0 || fm->selected >= fm->view_count) return;
    const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];

    if (e->type != FS_TYPE_DIR) return; /* no file viewer yet */

    char path[256];
    if (strcmp(e->name, "..") == 0) {
        /* Go to parent */
        strncpy(path, fm->cwd, 255);
        char *sl = strrchr(path, '/');
        if (sl && sl != path) *sl = '\0';
        else strcpy(path, "/");
    } else {
        build_path(fm->cwd, e->name, path);
    }
    fm_navigate(fm, path);
}

static void fm_go_up(FMData *fm)
{
    if (strcmp(fm->cwd, "/") == 0) return;
    char path[256];
    strncpy(path, fm->cwd, 255);
    char *sl = strrchr(path, '/');
    if (sl && sl != path) *sl = '\0';
    else strcpy(path, "/");
    fm_navigate(fm, path);
}

static void fm_go_back(FMData *fm)
{
    if (fm->hist_i <= 0) return;
    fm->hist_i--;
    strncpy(fm->cwd, fm->history[fm->hist_i], 255);
    fm->filter_len = 0;
    fm->filter[0] = '\0';
    fm_load(fm);
}

static void fm_go_fwd(FMData *fm)
{
    if (fm->hist_i >= fm->hist_n - 1) return;
    fm->hist_i++;
    strncpy(fm->cwd, fm->history[fm->hist_i], 255);
    fm->filter_len = 0;
    fm->filter[0] = '\0';
    fm_load(fm);
}

/*──────────────────────────────────────────────────────────────────
 * Context menu
 *────────────────────────────────────────────────────────────────*/
static void ctx_add(FMData *fm, const char *label, int action, int enabled, int sep)
{
    if (fm->ctx_n >= 8) return;
    fm->ctx_items[fm->ctx_n].label = label;
    fm->ctx_items[fm->ctx_n].action = action;
    fm->ctx_items[fm->ctx_n].enabled = enabled;
    fm->ctx_items[fm->ctx_n].sep_above = sep;
    fm->ctx_n++;
}

static int ctx_height(const FMData *fm)
{
    int h = 8;
    for (int i = 0; i < fm->ctx_n; i++) {
        if (fm->ctx_items[i].sep_above) h += CTX_SEP_H;
        h += CTX_ITEM_H;
    }
    return h;
}

static void build_ctx_file(FMData *fm)
{
    fm->ctx_n = 0;
    ctx_add(fm, "Open",    ACT_OPEN,    1, 0);
    ctx_add(fm, "Rename",  ACT_RENAME,  0, 1);
    ctx_add(fm, "Delete",  ACT_DELETE,  0, 0);
    ctx_add(fm, "Refresh", ACT_REFRESH, 1, 1);
}

static void build_ctx_bg(FMData *fm)
{
    int in_root = (strcmp(fm->cwd, "/") == 0);
    fm->ctx_n = 0;
    ctx_add(fm, "New File",  ACT_NEWFILE, in_root, 0);
    ctx_add(fm, "Refresh",   ACT_REFRESH, 1, 1);
    ctx_add(fm, fm->show_hidden ? "Hide Hidden" : "Show Hidden",
            ACT_HIDDEN, 1, 0);
}

static void ctx_exec(FMData *fm, int action)
{
    fm->ctx_vis = 0;
    switch (action) {
    case ACT_OPEN:
        if (fm->ctx_target >= 0) {
            fm->selected = fm->ctx_target;
            fm_open_selected(fm);
        }
        break;
    case ACT_NEWFILE:
        fm->dlg = DLG_NEWFILE;
        strcpy(fm->dlg_title, "New File");
        strcpy(fm->dlg_buf, "NEWFILE.TXT");
        fm->dlg_cur = 7; /* cursor before extension */
        break;
    case ACT_RENAME:
        fm->dlg = DLG_MSG;
        strcpy(fm->dlg_title, "Info");
        strcpy(fm->dlg_msg, "Rename not yet supported");
        break;
    case ACT_DELETE:
        fm->dlg = DLG_MSG;
        strcpy(fm->dlg_title, "Info");
        strcpy(fm->dlg_msg, "Delete not yet supported");
        break;
    case ACT_REFRESH:
        fm_load(fm);
        break;
    case ACT_HIDDEN:
        fm->show_hidden = !fm->show_hidden;
        rebuild_view(fm);
        break;
    }
}

/*──────────────────────────────────────────────────────────────────
 * Dialog confirm
 *────────────────────────────────────────────────────────────────*/
static void dlg_confirm(FMData *fm)
{
    if (fm->dlg == DLG_NEWFILE) {
        char path[256];
        build_path(fm->cwd, fm->dlg_buf, path);
        int fd = hal_fs_open(path, FS_O_CREATE | FS_O_WRITE);
        if (fd >= 0) {
            hal_fs_close(fd);
            fm_load(fm);
            fm->dlg = DLG_NONE;
        } else {
            fm->dlg = DLG_MSG;
            strcpy(fm->dlg_title, "Error");
            strcpy(fm->dlg_msg, "Could not create file");
        }
        return;
    }
    fm->dlg = DLG_NONE;
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — toolbar
 *────────────────────────────────────────────────────────────────*/
static void r_toolbar(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    /* Background */
    draw_filled_rect_blend(RECT(cr.x, cr.y, cr.w, TOOLBAR_H), tc->panel_bg);
    draw_line(cr.x, cr.y + TOOLBAR_H - 1,
              cr.x + cr.w, cr.y + TOOLBAR_H - 1, tc->panel_border);

    int bx = cr.x + 4, by = cr.y + 3, bs = 20;

    /* ◀ Back */
    Rect rb = RECT(bx, by, bs, bs);
    if (rect_contains(rb, mx, my))
        draw_filled_rounded_rect_blend(rb, 3, COLOR(255, 255, 255, 25));
    Color bc = fm->hist_i > 0 ? tc->text_primary : tc->text_muted;
    draw_text_centered(bx + bs / 2, by + 5, "<", bc, FONT_SIZE_SMALL);
    bx += bs + 2;

    /* ▶ Forward */
    rb = RECT(bx, by, bs, bs);
    if (rect_contains(rb, mx, my))
        draw_filled_rounded_rect_blend(rb, 3, COLOR(255, 255, 255, 25));
    bc = fm->hist_i < fm->hist_n - 1 ? tc->text_primary : tc->text_muted;
    draw_text_centered(bx + bs / 2, by + 5, ">", bc, FONT_SIZE_SMALL);
    bx += bs + 2;

    /* ▲ Up */
    rb = RECT(bx, by, bs, bs);
    if (rect_contains(rb, mx, my))
        draw_filled_rounded_rect_blend(rb, 3, COLOR(255, 255, 255, 25));
    bc = strcmp(fm->cwd, "/") != 0 ? tc->text_primary : tc->text_muted;
    draw_text_centered(bx + bs / 2, by + 5, "^", bc, FONT_SIZE_SMALL);
    bx += bs + 6;

    /* Vertical separator */
    draw_line(bx, by + 3, bx, by + bs - 3, tc->panel_border);
    bx += 6;

    /* Breadcrumb path area */
    int pw = cr.x + cr.w - bx - 6;
    draw_filled_rounded_rect_blend(RECT(bx, by, pw, bs), 3, COLOR(0, 0, 0, 40));

    /* Home icon at start */
    icon_draw(ICON_HOME, bx + 4, by + 4, 12, tc->accent);
    int tx = bx + 20;

    /* Parse and render path segments */
    if (strcmp(fm->cwd, "/") != 0) {
        char tmp[256];
        strncpy(tmp, fm->cwd, 255);
        tmp[255] = '\0';
        char *p = tmp + 1; /* skip leading / */
        while (p && *p) {
            char *sl = strchr(p, '/');
            if (sl) *sl = '\0';
            draw_text(tx, by + 5, ">", tc->text_muted, FONT_SIZE_SMALL);
            tx += 8;
            draw_text(tx, by + 5, p, tc->text_primary, FONT_SIZE_SMALL);
            tx += draw_text_width(p, FONT_SIZE_SMALL) + 4;
            if (sl) p = sl + 1;
            else break;
        }
    } else {
        draw_text(tx, by + 5, "/", tc->text_primary, FONT_SIZE_SMALL);
    }

    /* Filter indicator (right side of path bar) */
    if (fm->filter_len > 0) {
        int fw = draw_text_width(fm->filter, FONT_SIZE_SMALL) + 20;
        int fx = cr.x + cr.w - fw - 10;
        draw_filled_rounded_rect_blend(RECT(fx, by, fw, bs), 10, tc->accent_dim);
        icon_draw(ICON_SEARCH, fx + 3, by + 4, 12, tc->accent);
        draw_text(fx + 16, by + 5, fm->filter, tc->text_primary, FONT_SIZE_SMALL);
    }
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — sidebar
 *────────────────────────────────────────────────────────────────*/
static void r_sidebar(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int sx = cr.x;
    int sy = cr.y + TOOLBAR_H;
    int sw = SIDEBAR_W;
    int sh = cr.h - TOOLBAR_H - STATUSBAR_H;

    draw_filled_rect_blend(RECT(sx, sy, sw, sh), tc->sidebar_bg);
    draw_line(sx + sw - 1, sy, sx + sw - 1, sy + sh, tc->panel_border);

    int y = sy + 8;
    draw_text(sx + 10, y, "PLACES", tc->text_muted, FONT_SIZE_SMALL);
    y += 16;

    /* Quick-access items */
    struct { const char *label; const char *path; IconId icon; } qa[] = {
        { "Root",  "/",      ICON_HOME   },
        { "DOCS",  "/DOCS",  ICON_FOLDER },
    };

    for (int i = 0; i < 2; i++) {
        Rect r = RECT(sx + 4, y, sw - 8, 20);
        int active = (strcmp(fm->cwd, qa[i].path) == 0);
        int hover = rect_contains(r, mx, my);

        if (active)
            draw_filled_rounded_rect_blend(r, 3, tc->accent_dim);
        else if (hover)
            draw_filled_rounded_rect_blend(r, 3, COLOR(255, 255, 255, 15));

        Color ic = active ? tc->accent : tc->text_secondary;
        icon_draw(qa[i].icon, sx + 10, y + 3, 12, ic);
        draw_text(sx + 26, y + 5, qa[i].label,
                  active ? tc->accent : tc->text_primary, FONT_SIZE_SMALL);
        y += 22;
    }

    /* Separator */
    y += 6;
    widget_separator(sx + 8, y, sw - 16);
    y += 10;

    /* Disk info */
    draw_text(sx + 10, y, "DISK", tc->text_muted, FONT_SIZE_SMALL);
    y += 16;
    icon_draw(ICON_FILES, sx + 10, y + 2, 12, tc->text_secondary);
    draw_text(sx + 26, y + 4, "disk0", tc->text_secondary, FONT_SIZE_SMALL);
    y += 16;
    draw_text(sx + 26, y, "8 MB FAT16", tc->text_muted, FONT_SIZE_SMALL);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — column headers
 *────────────────────────────────────────────────────────────────*/
static void r_colhdr(FMData *fm, Rect cr, const ThemeColors *tc)
{
    int x = cr.x + SIDEBAR_W;
    int y = cr.y + TOOLBAR_H;
    int w = cr.w - SIDEBAR_W;

    draw_filled_rect_blend(RECT(x, y, w, COLHDR_H), COLOR(0, 0, 0, 30));
    draw_line(x, y + COLHDR_H - 1, x + w, y + COLHDR_H - 1, tc->panel_border);

    int nw = w * 55 / 100;
    int sw = w * 20 / 100;
    Color c;

    /* Name */
    c = (fm->sort_col == SORT_NAME) ? tc->accent : tc->text_muted;
    draw_text(x + 22, y + 3, "Name", c, FONT_SIZE_SMALL);
    if (fm->sort_col == SORT_NAME)
        draw_text(x + 50, y + 3, fm->sort_asc ? "v" : "^", tc->accent, FONT_SIZE_SMALL);

    /* Size */
    c = (fm->sort_col == SORT_SIZE) ? tc->accent : tc->text_muted;
    draw_text(x + nw + 4, y + 3, "Size", c, FONT_SIZE_SMALL);
    if (fm->sort_col == SORT_SIZE)
        draw_text(x + nw + 30, y + 3, fm->sort_asc ? "v" : "^", tc->accent, FONT_SIZE_SMALL);

    /* Type */
    c = (fm->sort_col == SORT_TYPE) ? tc->accent : tc->text_muted;
    draw_text(x + nw + sw + 4, y + 3, "Type", c, FONT_SIZE_SMALL);
    if (fm->sort_col == SORT_TYPE)
        draw_text(x + nw + sw + 30, y + 3, fm->sort_asc ? "v" : "^", tc->accent, FONT_SIZE_SMALL);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — file list
 *────────────────────────────────────────────────────────────────*/
static void r_filelist(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int x = cr.x + SIDEBAR_W;
    int y = cr.y + TOOLBAR_H + COLHDR_H;
    int w = cr.w - SIDEBAR_W;
    int h = cr.h - TOOLBAR_H - COLHDR_H - STATUSBAR_H;
    int vis = h / ROW_H;
    int nw = w * 55 / 100;
    int sw = w * 20 / 100;

    for (int vi = fm->scroll; vi < fm->view_count && vi < fm->scroll + vis; vi++) {
        int ry = y + (vi - fm->scroll) * ROW_H;
        Rect row = RECT(x, ry, w, ROW_H);
        int hover = rect_contains(row, mx, my);
        int sel = (vi == fm->selected);
        const fs_dirent_t *e = &fm->entries[fm->view[vi]];
        int is_dir = (e->type == FS_TYPE_DIR);
        int is_up = (strcmp(e->name, "..") == 0);

        /* Row background */
        if (sel)
            draw_filled_rect_blend(row, tc->accent_dim);
        else if (hover)
            draw_filled_rect_blend(row, COLOR(255, 255, 255, 12));

        /* Icon */
        Color ic = is_up ? tc->text_muted :
                   is_dir ? tc->accent : tc->text_secondary;
        icon_draw(is_dir ? ICON_FOLDER : ICON_FILE, x + 4, ry + 3, 14, ic);

        /* Name */
        Color nc = sel ? COLOR_WHITE : tc->text_primary;
        draw_text(x + 22, ry + 5, e->name, nc, FONT_SIZE_SMALL);

        /* Size column */
        if (!is_dir) {
            char sz[24];
            fmt_size(e->size, sz);
            draw_text(x + nw + 4, ry + 5, sz, tc->text_muted, FONT_SIZE_SMALL);
        }

        /* Type column */
        const char *type = is_up ? "Parent" : ftype_str(e);
        draw_text(x + nw + sw + 4, ry + 5, type, tc->text_muted, FONT_SIZE_SMALL);

        /* Subtle row separator */
        if (vi < fm->scroll + vis - 1)
            draw_line(x + 4, ry + ROW_H - 1, x + w - 4,
                      ry + ROW_H - 1, COLOR(255, 255, 255, 8));
    }

    /* Empty folder message */
    if (fm->view_count == 0)
        draw_text_centered(x + w / 2, y + h / 2 - 5,
                          "Empty folder", tc->text_muted, FONT_SIZE_SMALL);

    /* Scrollbar */
    if (fm->view_count > vis && vis > 0) {
        int th = max_i(16, h * vis / fm->view_count);
        int max_sc = fm->view_count - vis;
        int ty = y + (h - th) * fm->scroll / max_i(1, max_sc);
        draw_filled_rounded_rect_blend(RECT(x + w - 5, ty, 3, th), 2,
                                        COLOR(255, 255, 255, 40));
    }
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — status bar
 *────────────────────────────────────────────────────────────────*/
static void r_status(FMData *fm, Rect cr, const ThemeColors *tc)
{
    int y = cr.y + cr.h - STATUSBAR_H;

    draw_filled_rect_blend(RECT(cr.x, y, cr.w, STATUSBAR_H), tc->panel_bg);
    draw_line(cr.x, y, cr.x + cr.w, y, tc->panel_border);

    /* Item count */
    char buf[32];
    itoa(fm->view_count, buf, 10);
    strcat(buf, " items");
    draw_text(cr.x + 8, y + 4, buf, tc->text_muted, FONT_SIZE_SMALL);

    /* Selected file name (center) */
    if (fm->selected >= 0 && fm->selected < fm->view_count) {
        const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
        draw_text_centered(cr.x + cr.w / 2, y + 4,
                          e->name, tc->text_secondary, FONT_SIZE_SMALL);
    }

    /* Current path (right) */
    draw_text_right(cr.x + cr.w - 8, y + 4, fm->cwd,
                    tc->text_muted, FONT_SIZE_SMALL);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — context menu
 *────────────────────────────────────────────────────────────────*/
static void r_ctx(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int mh = ctx_height(fm);

    /* Content-relative → absolute, clamped to stay in bounds */
    int ax = cr.x + fm->ctx_x;
    int ay = cr.y + fm->ctx_y;
    if (ax + CTX_W > cr.x + cr.w) ax = cr.x + cr.w - CTX_W;
    if (ay + mh > cr.y + cr.h)    ay = cr.y + cr.h - mh;

    /* Shadow + background */
    draw_filled_rounded_rect_blend(RECT(ax + 2, ay + 2, CTX_W, mh), 5,
                                    COLOR(0, 0, 0, 100));
    draw_filled_rounded_rect(RECT(ax, ay, CTX_W, mh), 4, tc->window_bg);
    draw_rounded_rect(RECT(ax, ay, CTX_W, mh), 4, tc->panel_border);

    /* Items */
    int iy = ay + 4;
    for (int i = 0; i < fm->ctx_n; i++) {
        if (fm->ctx_items[i].sep_above) {
            draw_line(ax + 8, iy + CTX_SEP_H / 2, ax + CTX_W - 8,
                      iy + CTX_SEP_H / 2, tc->panel_border);
            iy += CTX_SEP_H;
        }

        Rect ir = RECT(ax + 4, iy, CTX_W - 8, CTX_ITEM_H);
        int hov = rect_contains(ir, mx, my) && fm->ctx_items[i].enabled;

        if (hov)
            draw_filled_rounded_rect_blend(ir, 3, tc->accent_dim);

        Color c = fm->ctx_items[i].enabled
                  ? (hov ? COLOR_WHITE : tc->text_primary)
                  : tc->text_muted;
        draw_text(ax + 12, iy + 5, fm->ctx_items[i].label,
                  c, FONT_SIZE_SMALL);

        iy += CTX_ITEM_H;
    }
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — dialog
 *────────────────────────────────────────────────────────────────*/
static void r_dialog(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    /* Dim overlay */
    draw_filled_rect_blend(cr, COLOR(0, 0, 0, 100));

    int dw = 220;
    int dh = (fm->dlg == DLG_MSG) ? 85 : 105;
    int dx = cr.x + (cr.w - dw) / 2;
    int dy = cr.y + (cr.h - dh) / 2;

    /* Dialog frame */
    draw_filled_rounded_rect(RECT(dx, dy, dw, dh), 6, tc->window_bg);
    draw_rounded_rect(RECT(dx, dy, dw, dh), 6, tc->accent_dim);

    /* Title */
    draw_text(dx + 12, dy + 10, fm->dlg_title,
              tc->text_primary, FONT_SIZE_SMALL);
    draw_line(dx + 8, dy + 24, dx + dw - 8, dy + 24, tc->panel_border);

    if (fm->dlg == DLG_MSG) {
        /* Message text */
        draw_text(dx + 12, dy + 34, fm->dlg_msg,
                  tc->text_secondary, FONT_SIZE_SMALL);
        /* OK button */
        Rect ok = RECT(dx + dw - 60, dy + dh - 30, 48, 22);
        widget_button(ok, "OK", BTN_PRIMARY, mx, my, 0);
    } else {
        /* Text input field */
        Rect inp = RECT(dx + 12, dy + 32, dw - 24, 22);
        draw_filled_rounded_rect(inp, 3, COLOR(0, 0, 0, 60));
        draw_rounded_rect(inp, 3, tc->accent);
        draw_text(dx + 16, dy + 38, fm->dlg_buf,
                  tc->text_primary, FONT_SIZE_SMALL);

        /* Blinking cursor */
        if ((hal_get_ticks() / 500) % 2 == 0) {
            int cx = dx + 16 + fm->dlg_cur * 6;
            draw_filled_rect(RECT(cx, dy + 36, 1, 12), tc->accent);
        }

        /* Buttons */
        Rect cancel = RECT(dx + dw - 120, dy + dh - 30, 52, 22);
        Rect ok = RECT(dx + dw - 60, dy + dh - 30, 48, 22);
        widget_button(cancel, "Cancel", BTN_SECONDARY, mx, my, 0);
        widget_button(ok, "Create", BTN_PRIMARY, mx, my, 0);
    }
}

/*──────────────────────────────────────────────────────────────────
 * Main render
 *────────────────────────────────────────────────────────────────*/
static void fm_render(AppContent *self, Rect cr)
{
    FMData *fm = (FMData *)self->data;
    fm->cw = cr.w;
    fm->ch = cr.h;
    const ThemeColors *tc = theme_colors();
    int mx, my;
    hal_input_mouse_pos(&mx, &my);

    r_toolbar(fm, cr, tc, mx, my);
    r_sidebar(fm, cr, tc, mx, my);
    r_colhdr(fm, cr, tc);
    r_filelist(fm, cr, tc, mx, my);
    r_status(fm, cr, tc);

    if (fm->ctx_vis)
        r_ctx(fm, cr, tc, mx, my);
    if (fm->dlg != DLG_NONE)
        r_dialog(fm, cr, tc, mx, my);
}

/*──────────────────────────────────────────────────────────────────
 * Mouse handler
 *────────────────────────────────────────────────────────────────*/
static void fm_on_mouse_down(AppContent *self, int x, int y)
{
    FMData *fm = (FMData *)self->data;
    int btn = wm_last_mouse_button();

    /* ── Dialog is open ───────────────────────────────────────── */
    if (fm->dlg != DLG_NONE) {
        int dw = 220;
        int dh = (fm->dlg == DLG_MSG) ? 85 : 105;
        int dx = (fm->cw - dw) / 2;
        int dy = (fm->ch - dh) / 2;

        if (fm->dlg == DLG_MSG) {
            /* OK button or anywhere */
            fm->dlg = DLG_NONE;
        } else {
            /* Cancel button */
            if (x >= dx + dw - 120 && x < dx + dw - 68 &&
                y >= dy + dh - 30  && y < dy + dh - 8) {
                fm->dlg = DLG_NONE;
                return;
            }
            /* Create/OK button */
            if (x >= dx + dw - 60 && x < dx + dw - 12 &&
                y >= dy + dh - 30 && y < dy + dh - 8) {
                dlg_confirm(fm);
                return;
            }
            /* Click outside dialog = cancel */
            if (x < dx || x > dx + dw || y < dy || y > dy + dh) {
                fm->dlg = DLG_NONE;
                return;
            }
        }
        return;
    }

    /* ── Context menu is open ─────────────────────────────────── */
    if (fm->ctx_vis) {
        int mh = ctx_height(fm);
        int cx = fm->ctx_x, cy = fm->ctx_y;
        /* Clamp same as render */
        if (cx + CTX_W > fm->cw) cx = fm->cw - CTX_W;
        if (cy + mh > fm->ch)    cy = fm->ch - mh;

        if (x >= cx && x < cx + CTX_W && y >= cy && y < cy + mh) {
            int iy = cy + 4;
            for (int i = 0; i < fm->ctx_n; i++) {
                if (fm->ctx_items[i].sep_above) iy += CTX_SEP_H;
                if (y >= iy && y < iy + CTX_ITEM_H) {
                    if (fm->ctx_items[i].enabled)
                        ctx_exec(fm, fm->ctx_items[i].action);
                    fm->ctx_vis = 0;
                    return;
                }
                iy += CTX_ITEM_H;
            }
        }
        fm->ctx_vis = 0;
        return;
    }

    /* ── Right-click → context menu ───────────────────────────── */
    if (btn == 3) {
        int list_y0 = TOOLBAR_H + COLHDR_H;
        int list_h = fm->ch - TOOLBAR_H - COLHDR_H - STATUSBAR_H;
        int vis = list_h / ROW_H;

        if (x >= SIDEBAR_W && y >= list_y0 &&
            y < list_y0 + vis * ROW_H) {
            int row = (y - list_y0) / ROW_H + fm->scroll;
            if (row >= 0 && row < fm->view_count) {
                fm->selected = row;
                fm->ctx_target = row;
                build_ctx_file(fm);
            } else {
                fm->ctx_target = -1;
                build_ctx_bg(fm);
            }
        } else {
            fm->ctx_target = -1;
            build_ctx_bg(fm);
        }

        /* Store position and clamp */
        fm->ctx_x = x;
        fm->ctx_y = y;
        int mh = ctx_height(fm);
        if (fm->ctx_x + CTX_W > fm->cw) fm->ctx_x = fm->cw - CTX_W;
        if (fm->ctx_y + mh > fm->ch)    fm->ctx_y = fm->ch - mh;
        if (fm->ctx_x < 0) fm->ctx_x = 0;
        if (fm->ctx_y < 0) fm->ctx_y = 0;
        fm->ctx_vis = 1;
        return;
    }

    /* ── Left-click only below ────────────────────────────────── */

    /* Toolbar buttons */
    if (y < TOOLBAR_H) {
        int bx = 4, bs = 20;
        if (x >= bx && x < bx + bs) { fm_go_back(fm); return; }
        bx += bs + 2;
        if (x >= bx && x < bx + bs) { fm_go_fwd(fm); return; }
        bx += bs + 2;
        if (x >= bx && x < bx + bs) { fm_go_up(fm); return; }
        return;
    }

    /* Sidebar */
    if (x < SIDEBAR_W && y >= TOOLBAR_H) {
        int sy = TOOLBAR_H + 24; /* after "PLACES" label */
        const char *paths[] = {"/", "/DOCS"};
        for (int i = 0; i < 2; i++) {
            if (y >= sy && y < sy + 22) {
                fm_navigate(fm, paths[i]);
                return;
            }
            sy += 22;
        }
        return;
    }

    /* Column headers */
    if (y >= TOOLBAR_H && y < TOOLBAR_H + COLHDR_H && x >= SIDEBAR_W) {
        int lx = x - SIDEBAR_W;
        int w = fm->cw - SIDEBAR_W;
        int nw = w * 55 / 100;
        int sw = w * 20 / 100;
        int col;
        if (lx < nw)          col = SORT_NAME;
        else if (lx < nw + sw) col = SORT_SIZE;
        else                    col = SORT_TYPE;

        if (fm->sort_col == col)
            fm->sort_asc = !fm->sort_asc;
        else {
            fm->sort_col = col;
            fm->sort_asc = 1;
        }
        rebuild_view(fm);
        return;
    }

    /* File list */
    int list_y0 = TOOLBAR_H + COLHDR_H;
    int list_h = fm->ch - TOOLBAR_H - COLHDR_H - STATUSBAR_H;
    if (y >= list_y0 && y < list_y0 + list_h && x >= SIDEBAR_W) {
        int row = (y - list_y0) / ROW_H + fm->scroll;
        if (row >= 0 && row < fm->view_count) {
            /* Double-click detection */
            uint32_t now = hal_get_ticks();
            if (row == fm->dbl_item &&
                (now - fm->dbl_time) < DBLCLICK_MS) {
                fm->selected = row;
                fm_open_selected(fm);
                fm->dbl_item = -1;
                return;
            }
            fm->dbl_time = now;
            fm->dbl_item = row;
            fm->selected = row;
        } else {
            fm->selected = -1;
        }
    }
}

/*──────────────────────────────────────────────────────────────────
 * Keyboard handler
 *────────────────────────────────────────────────────────────────*/
static void fm_on_key_down(AppContent *self, uint16_t key, uint16_t mod)
{
    FMData *fm = (FMData *)self->data;

    /* ── Dialog mode ──────────────────────────────────────────── */
    if (fm->dlg != DLG_NONE) {
        if (key == HAL_KEY_ESCAPE) {
            fm->dlg = DLG_NONE;
            return;
        }
        if (key == HAL_KEY_RETURN) {
            if (fm->dlg == DLG_MSG)
                fm->dlg = DLG_NONE;
            else
                dlg_confirm(fm);
            return;
        }
        /* Text editing in dialog input */
        if (fm->dlg != DLG_MSG) {
            if (key == HAL_KEY_BACKSPACE && fm->dlg_cur > 0) {
                int len = (int)strlen(fm->dlg_buf);
                memmove(fm->dlg_buf + fm->dlg_cur - 1,
                        fm->dlg_buf + fm->dlg_cur,
                        (unsigned)(len - fm->dlg_cur + 1));
                fm->dlg_cur--;
                return;
            }
            if (key == HAL_KEY_LEFT && fm->dlg_cur > 0) {
                fm->dlg_cur--;
                return;
            }
            if (key == HAL_KEY_RIGHT && fm->dlg_cur < (int)strlen(fm->dlg_buf)) {
                fm->dlg_cur++;
                return;
            }
            if (key == HAL_KEY_HOME) { fm->dlg_cur = 0; return; }
            if (key == HAL_KEY_END) {
                fm->dlg_cur = (int)strlen(fm->dlg_buf);
                return;
            }
        }
        return;
    }

    /* Dismiss context menu */
    if (fm->ctx_vis && key == HAL_KEY_ESCAPE) {
        fm->ctx_vis = 0;
        return;
    }

    /* ── Navigation ───────────────────────────────────────────── */
    if (key == HAL_KEY_BACKSPACE) { fm_go_up(fm); return; }
    if (key == HAL_KEY_LEFT && (mod & HAL_MOD_ALT)) { fm_go_back(fm); return; }
    if (key == HAL_KEY_RIGHT && (mod & HAL_MOD_ALT)) { fm_go_fwd(fm); return; }

    /* Enter = open selected */
    if (key == HAL_KEY_RETURN && fm->selected >= 0) {
        fm_open_selected(fm);
        return;
    }

    /* F5 = refresh */
    if (key == HAL_KEY_F5) { fm_load(fm); return; }

    /* Ctrl+H = toggle hidden */
    if (key == HAL_KEY_H && (mod & HAL_MOD_CTRL)) {
        fm->show_hidden = !fm->show_hidden;
        rebuild_view(fm);
        return;
    }

    /* Ctrl+N = new file */
    if (key == HAL_KEY_N && (mod & HAL_MOD_CTRL)) {
        if (strcmp(fm->cwd, "/") == 0) {
            fm->dlg = DLG_NEWFILE;
            strcpy(fm->dlg_title, "New File");
            strcpy(fm->dlg_buf, "NEWFILE.TXT");
            fm->dlg_cur = 7;
        }
        return;
    }

    /* F2 = rename (shows info message) */
    if (key == HAL_KEY_F2 && fm->selected >= 0) {
        fm->dlg = DLG_MSG;
        strcpy(fm->dlg_title, "Info");
        strcpy(fm->dlg_msg, "Rename not yet supported");
        return;
    }

    /* Delete key */
    if (key == HAL_KEY_DELETE && fm->selected >= 0) {
        fm->dlg = DLG_MSG;
        strcpy(fm->dlg_title, "Info");
        strcpy(fm->dlg_msg, "Delete not yet supported");
        return;
    }

    /* Escape = clear filter or deselect */
    if (key == HAL_KEY_ESCAPE) {
        if (fm->filter_len > 0) {
            fm->filter_len = 0;
            fm->filter[0] = '\0';
            rebuild_view(fm);
        } else {
            fm->selected = -1;
        }
        return;
    }

    /* ── Arrow key navigation ─────────────────────────────────── */
    int vis = (fm->ch - TOOLBAR_H - COLHDR_H - STATUSBAR_H) / ROW_H;
    if (vis <= 0) vis = 1;

    if (key == HAL_KEY_UP) {
        if (fm->selected > 0) fm->selected--;
        else if (fm->view_count > 0) fm->selected = 0;
        if (fm->selected < fm->scroll) fm->scroll = fm->selected;
        return;
    }
    if (key == HAL_KEY_DOWN) {
        if (fm->selected < fm->view_count - 1) fm->selected++;
        else if (fm->view_count > 0) fm->selected = fm->view_count - 1;
        if (fm->selected >= fm->scroll + vis)
            fm->scroll = fm->selected - vis + 1;
        return;
    }
    if (key == HAL_KEY_HOME) {
        fm->selected = 0;
        fm->scroll = 0;
        return;
    }
    if (key == HAL_KEY_END) {
        fm->selected = fm->view_count - 1;
        if (fm->selected >= vis)
            fm->scroll = fm->selected - vis + 1;
        return;
    }
    if (key == HAL_KEY_PAGEUP) {
        fm->selected = max_i(0, fm->selected - vis);
        fm->scroll = max_i(0, fm->scroll - vis);
        return;
    }
    if (key == HAL_KEY_PAGEDOWN) {
        fm->selected = min_i(fm->view_count - 1, fm->selected + vis);
        if (fm->selected >= fm->scroll + vis)
            fm->scroll = fm->selected - vis + 1;
        return;
    }
    (void)mod;
}

/*──────────────────────────────────────────────────────────────────
 * Text input handler (typing characters)
 *────────────────────────────────────────────────────────────────*/
static void fm_on_text(AppContent *self, char ch)
{
    FMData *fm = (FMData *)self->data;

    /* Dialog text input */
    if (fm->dlg != DLG_NONE && fm->dlg != DLG_MSG) {
        if (ch >= 32 && ch < 127 && fm->dlg_cur < 62) {
            int len = (int)strlen(fm->dlg_buf);
            memmove(fm->dlg_buf + fm->dlg_cur + 1,
                    fm->dlg_buf + fm->dlg_cur,
                    (unsigned)(len - fm->dlg_cur + 1));
            fm->dlg_buf[fm->dlg_cur++] = ch;
        }
        return;
    }

    /* Type-ahead filter (when no dialog) */
    if (ch >= 32 && ch < 127 && fm->filter_len < 30 && !fm->dlg) {
        fm->filter[fm->filter_len++] = ch;
        fm->filter[fm->filter_len] = '\0';
        rebuild_view(fm);
        fm->selected = fm->view_count > 0 ? 0 : -1;
        fm->scroll = 0;
    }
}

/*──────────────────────────────────────────────────────────────────
 * Scroll handler
 *────────────────────────────────────────────────────────────────*/
static void fm_on_scroll(AppContent *self, int x, int y, int sy)
{
    FMData *fm = (FMData *)self->data;
    (void)x; (void)y;

    fm->scroll -= sy * 3;
    if (fm->scroll < 0) fm->scroll = 0;

    int vis = (fm->ch - TOOLBAR_H - COLHDR_H - STATUSBAR_H) / ROW_H;
    int max_scroll = fm->view_count - vis;
    if (max_scroll < 0) max_scroll = 0;
    if (fm->scroll > max_scroll) fm->scroll = max_scroll;
}

/*──────────────────────────────────────────────────────────────────
 * Lifecycle
 *────────────────────────────────────────────────────────────────*/
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
    fm->sort_asc = 1;
    fm->selected = -1;
    fm->dbl_item = -1;
    fm->ctx_target = -1;
    fm->cw = 498;  /* reasonable defaults until first render */
    fm->ch = 321;

    /* Seed history */
    strcpy(fm->history[0], "/");
    fm->hist_n = 1;
    fm->hist_i = 0;

    fm_load(fm);

    app->render = fm_render;
    app->on_mouse_down = fm_on_mouse_down;
    app->on_key_down = fm_on_key_down;
    app->on_text_input = fm_on_text;
    app->on_scroll = fm_on_scroll;
    app->destroy = fm_destroy;
    app->data = fm;

    return app;
}
