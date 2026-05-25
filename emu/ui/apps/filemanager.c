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
#include "../../hal/hal_image.h"
#include "terminal.h"
#include "imageviewer.h"

/*──────────────────────────────────────────────────────────────────
 * Layout constants
 *────────────────────────────────────────────────────────────────*/
#define FM_MAX_ENTRIES  128
#define FM_MAX_HIST     16
#define TOOLBAR_H       26
#define SIDEBAR_W       110
#define COLHDR_H        16
#define ROW_H           20
#define STATUSBAR_H     20
#define CTX_W           155
#define CTX_ITEM_H      20
#define CTX_SEP_H       8
#define DBLCLICK_MS     400
#define PREVIEW_W       160
#define GRID_CELL_W     80
#define GRID_CELL_H     70

/* Sort columns */
#define SORT_NAME  0
#define SORT_SIZE  1
#define SORT_TYPE  2

/* Dialog types */
#define DLG_NONE      0
#define DLG_NEWFILE   1
#define DLG_NEWFOLDER 2
#define DLG_RENAME    3
#define DLG_DELETE    4
#define DLG_MSG       5
#define DLG_PROPS     6

/* Context menu actions */
#define ACT_OPEN       0
#define ACT_NEWFILE    1
#define ACT_NEWFOLDER  2
#define ACT_RENAME     3
#define ACT_DELETE     4
#define ACT_REFRESH    5
#define ACT_HIDDEN     6
#define ACT_CUT        7
#define ACT_COPY       8
#define ACT_PASTE      9
#define ACT_SELECTALL  10
#define ACT_PROPS      11
#define ACT_GRIDVIEW   12

/* View modes */
#define VIEW_LIST 0
#define VIEW_GRID 1

/*──────────────────────────────────────────────────────────────────
 * Types
 *────────────────────────────────────────────────────────────────*/
typedef struct {
    const char *label;
    const char *shortcut;
    int  action;
    int  enabled;
    int  sep_above;
    int  danger;    /* render in red */
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

    /* Selection — multi-select support */
    int         selected;       /* primary cursor, -1 = none */
    uint8_t     sel_flags[FM_MAX_ENTRIES]; /* 1=selected for each view idx */
    int         sel_count;      /* number selected */
    int         sel_anchor;     /* for shift-click range select */
    int         scroll;

    /* Sort / filter / view */
    int         sort_col;
    int         sort_asc;
    int         show_hidden;
    int         view_mode;      /* VIEW_LIST or VIEW_GRID */
    int         show_preview;   /* preview panel toggle */

    /* Clipboard */
    char        clip_paths[16][256];
    int         clip_count;
    int         clip_cut;       /* 1=cut, 0=copy */

    /* Context menu */
    int         ctx_vis;
    int         ctx_x, ctx_y;   /* content-relative */
    int         ctx_target;     /* view index, -1 = background */
    CtxItem     ctx_items[14];
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

    /* Toast notification */
    char        toast_msg[64];
    uint32_t    toast_time;

    /* Image preview cache */
    hal_image_t preview_thumb;
    int         preview_for_idx;   /* view index, -1 = none */

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
    if (strcmp(dot, ".txt") == 0) return "Text";
    if (strcmp(dot, ".c") == 0)   return "C Source";
    if (strcmp(dot, ".h") == 0)   return "Header";
    if (strcmp(dot, ".s") == 0)   return "Assembly";
    if (strcmp(dot, ".sh") == 0)  return "Script";
    if (strcmp(dot, ".md") == 0)  return "Markdown";
    if (strcmp(dot, ".bin") == 0) return "Binary";
    if (strcmp(dot, ".img") == 0) return "Disk Img";
    if (strcmp(dot, ".cfg") == 0) return "Config";
    if (strcmp(dot, ".tar") == 0) return "Archive";
    if (strcmp(dot, ".gz") == 0)  return "Archive";
    if (strcmp(dot, ".bmp") == 0)  return "BMP Image";
    if (strcmp(dot, ".png") == 0)  return "PNG Image";
    if (strcmp(dot, ".jpg") == 0)  return "JPEG Image";
    if (strcmp(dot, ".jpeg") == 0) return "JPEG Image";
    if (strcmp(dot, ".gif") == 0)  return "GIF Image";
    if (strcmp(dot, ".tga") == 0)  return "TGA Image";
    if (strcmp(dot, ".psd") == 0)  return "PSD Image";
    if (strcmp(dot, ".ppm") == 0)  return "PPM Image";
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

static void show_toast(FMData *fm, const char *msg)
{
    strncpy(fm->toast_msg, msg, 63);
    fm->toast_msg[63] = '\0';
    fm->toast_time = hal_get_ticks();
}

static void clear_selection(FMData *fm)
{
    memset(fm->sel_flags, 0, sizeof(fm->sel_flags));
    fm->sel_count = 0;
    fm->selected = -1;
    /* Invalidate image preview cache */
    hal_image_free(&fm->preview_thumb);
    fm->preview_for_idx = -1;
}

static void select_single(FMData *fm, int vi)
{
    memset(fm->sel_flags, 0, sizeof(fm->sel_flags));
    fm->sel_count = 0;
    if (vi >= 0 && vi < fm->view_count) {
        fm->sel_flags[vi] = 1;
        fm->sel_count = 1;
        fm->selected = vi;
        fm->sel_anchor = vi;
    } else {
        fm->selected = -1;
    }
}

static void toggle_select(FMData *fm, int vi)
{
    if (vi < 0 || vi >= fm->view_count) return;
    if (fm->sel_flags[vi]) {
        fm->sel_flags[vi] = 0;
        fm->sel_count--;
    } else {
        fm->sel_flags[vi] = 1;
        fm->sel_count++;
    }
    fm->selected = vi;
    fm->sel_anchor = vi;
}

static void range_select(FMData *fm, int vi)
{
    if (vi < 0 || vi >= fm->view_count) return;
    memset(fm->sel_flags, 0, sizeof(fm->sel_flags));
    fm->sel_count = 0;
    int a = fm->sel_anchor < vi ? fm->sel_anchor : vi;
    int b = fm->sel_anchor > vi ? fm->sel_anchor : vi;
    if (a < 0) a = 0;
    for (int i = a; i <= b && i < fm->view_count; i++) {
        fm->sel_flags[i] = 1;
        fm->sel_count++;
    }
    fm->selected = vi;
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
    /* Reset selection flags for valid range */
    for (int i = fm->view_count; i < FM_MAX_ENTRIES; i++)
        fm->sel_flags[i] = 0;
}

/*──────────────────────────────────────────────────────────────────
 * Directory operations
 *────────────────────────────────────────────────────────────────*/
static void fm_load(FMData *fm)
{
    fm->entry_count = 0;
    clear_selection(fm);
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

static int is_text_file(const char *name)
{
    static const char *exts[] = {
        /* Documents & markup */
        ".txt", ".text", ".md", ".markdown", ".rst", ".adoc",
        ".html", ".htm", ".xml", ".xhtml", ".svg",
        ".css", ".scss", ".sass", ".less",
        /* Data formats */
        ".json", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf",
        ".config", ".env", ".properties", ".csv", ".tsv",
        /* Programming — C family */
        ".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hh",
        /* Programming — scripting */
        ".py", ".rb", ".pl", ".lua", ".sh", ".bash", ".zsh",
        ".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs",
        ".php", ".r", ".swift", ".kt", ".kts",
        /* Programming — systems */
        ".rs", ".go", ".java", ".scala", ".zig", ".nim", ".d",
        /* Assembly / low-level */
        ".s", ".asm", ".ld",
        /* Build / DevOps */
        ".mk", ".cmake", ".gradle", ".sbt",
        ".dockerfile", ".tf", ".hcl",
        /* Database */
        ".sql",
        /* Misc */
        ".log", ".diff", ".patch", ".bat", ".ps1", ".vim",
        ".gitignore", ".editorconfig",
        ".vue", ".svelte",
        (void *)0
    };
    const char *dot = strrchr(name, '.');
    if (!dot) {
        /* No extension — check common extensionless text files */
        if (strcmp(name, "Makefile") == 0) return 1;
        if (strcmp(name, "README") == 0) return 1;
        if (strcmp(name, "LICENSE") == 0) return 1;
        if (strcmp(name, "CHANGELOG") == 0) return 1;
        return 0;
    }
    for (int i = 0; exts[i]; i++) {
        if (strcmp(dot, exts[i]) == 0) return 1;
    }
    return 0;
}

static int is_image_file(const char *name)
{
    return hal_image_is_supported(name);
}

static void fm_open_selected(FMData *fm)
{
    if (fm->selected < 0 || fm->selected >= fm->view_count) return;
    const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];

    if (e->type == FS_TYPE_DIR) {
        char path[256];
        if (strcmp(e->name, "..") == 0) {
            strncpy(path, fm->cwd, 255);
            char *sl = strrchr(path, '/');
            if (sl && sl != path) *sl = '\0';
            else strcpy(path, "/");
        } else {
            build_path(fm->cwd, e->name, path);
        }
        fm_navigate(fm, path);
        return;
    }

    /* Image file → open in image viewer */
    if (is_image_file(e->name)) {
        char full[256];
        build_path(fm->cwd, e->name, full);
        AppContent *content = imageviewer_create_with_file(full);
        if (content) {
            char title[64];
            strncpy(title, e->name, 63);
            title[63] = '\0';
            wm_open(title, 100, 60, 520, 400,
                    WIN_CLOSABLE | WIN_RESIZABLE, -1, content);
        }
        return;
    }

    /* Text file → open in terminal with nano */
    if (is_text_file(e->name)) {
        char full[256];
        build_path(fm->cwd, e->name, full);
        AppContent *content = terminal_create_with_nano(full);
        if (content) {
            char title[64];
            strcpy(title, "nano: ");
            strncpy(title + 6, e->name, 57);
            title[63] = '\0';
            wm_open(title, 120, 80, 500, 350,
                    WIN_CLOSABLE | WIN_RESIZABLE, -1, content);
        }
        return;
    }
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
 * Clipboard operations
 *────────────────────────────────────────────────────────────────*/
static void fm_clip_copy(FMData *fm, int cut)
{
    fm->clip_count = 0;
    fm->clip_cut = cut;
    for (int i = 0; i < fm->view_count && fm->clip_count < 16; i++) {
        if (fm->sel_flags[i]) {
            const fs_dirent_t *e = &fm->entries[fm->view[i]];
            if (strcmp(e->name, "..") == 0) continue;
            build_path(fm->cwd, e->name, fm->clip_paths[fm->clip_count]);
            fm->clip_count++;
        }
    }
    show_toast(fm, cut ? "Cut to clipboard" : "Copied to clipboard");
}

static void fm_copy_file(const char *src_path, const char *dst_path)
{
    int src_fd = hal_fs_open(src_path, FS_O_READ);
    if (src_fd < 0) return;

    int dst_fd = hal_fs_open(dst_path, FS_O_CREATE | FS_O_WRITE);
    if (dst_fd < 0) { hal_fs_close(src_fd); return; }

    char buf[512];
    int n;
    while ((n = hal_fs_read(src_fd, buf, sizeof(buf))) > 0)
        hal_fs_write(dst_fd, buf, n);

    hal_fs_close(src_fd);
    hal_fs_close(dst_fd);
}

static void fm_clip_paste(FMData *fm)
{
    if (fm->clip_count <= 0) return;

    int ok = 0;
    for (int i = 0; i < fm->clip_count; i++) {
        const char *name = strrchr(fm->clip_paths[i], '/');
        if (name) name++;
        else name = fm->clip_paths[i];

        char dest[256];
        build_path(fm->cwd, name, dest);

        if (fm->clip_cut) {
            if (hal_fs_rename(fm->clip_paths[i], dest) == 0)
                ok++;
        } else {
            /* Copy: read source file, write to destination */
            fs_stat_t st;
            if (hal_fs_stat(fm->clip_paths[i], &st) == 0 &&
                st.type == FS_TYPE_FILE) {
                fm_copy_file(fm->clip_paths[i], dest);
                ok++;
            }
        }
    }

    char msg[64];
    strcpy(msg, fm->clip_cut ? "Moved " : "Copied ");
    char num[8];
    itoa(ok, num, 10);
    strcat(msg, num);
    strcat(msg, " item(s)");
    show_toast(fm, msg);

    if (fm->clip_cut)
        fm->clip_count = 0;

    fm_load(fm);
}

/*──────────────────────────────────────────────────────────────────
 * Context menu
 *────────────────────────────────────────────────────────────────*/
static void ctx_add(FMData *fm, const char *label, const char *sc,
                    int action, int enabled, int sep, int danger)
{
    if (fm->ctx_n >= 14) return;
    fm->ctx_items[fm->ctx_n].label = label;
    fm->ctx_items[fm->ctx_n].shortcut = sc;
    fm->ctx_items[fm->ctx_n].action = action;
    fm->ctx_items[fm->ctx_n].enabled = enabled;
    fm->ctx_items[fm->ctx_n].sep_above = sep;
    fm->ctx_items[fm->ctx_n].danger = danger;
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
    ctx_add(fm, "Open",       "Enter",  ACT_OPEN,    1, 0, 0);
    ctx_add(fm, "Cut",        "Ctrl+X", ACT_CUT,     1, 1, 0);
    ctx_add(fm, "Copy",       "Ctrl+C", ACT_COPY,    1, 0, 0);
    ctx_add(fm, "Paste",      "Ctrl+V", ACT_PASTE,   fm->clip_count > 0, 0, 0);
    ctx_add(fm, "Rename",     "F2",     ACT_RENAME,  1, 1, 0);
    ctx_add(fm, "Delete",     "Del",    ACT_DELETE,  1, 0, 1);
    ctx_add(fm, "Properties", "",       ACT_PROPS,   1, 1, 0);
}

static void build_ctx_bg(FMData *fm)
{
    fm->ctx_n = 0;
    ctx_add(fm, "New File",   "Ctrl+N",   ACT_NEWFILE,   1, 0, 0);
    ctx_add(fm, "New Folder", "Ctrl+Sh+N", ACT_NEWFOLDER, 1, 0, 0);
    ctx_add(fm, "Paste",      "Ctrl+V",   ACT_PASTE,     fm->clip_count > 0, 1, 0);
    ctx_add(fm, "Select All", "Ctrl+A",   ACT_SELECTALL, 1, 0, 0);
    ctx_add(fm, "Refresh",    "F5",       ACT_REFRESH,   1, 1, 0);
    ctx_add(fm, fm->show_hidden ? "Hide Hidden" : "Show Hidden",
            "Ctrl+H", ACT_HIDDEN, 1, 0, 0);
    ctx_add(fm, fm->view_mode == VIEW_LIST ? "Grid View" : "List View",
            "", ACT_GRIDVIEW, 1, 1, 0);
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
        fm->dlg_cur = 7;
        break;
    case ACT_NEWFOLDER:
        fm->dlg = DLG_NEWFOLDER;
        strcpy(fm->dlg_title, "New Folder");
        strcpy(fm->dlg_buf, "NEWFOLDER");
        fm->dlg_cur = 9;
        break;
    case ACT_RENAME:
        if (fm->selected >= 0) {
            const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
            fm->dlg = DLG_RENAME;
            strcpy(fm->dlg_title, "Rename");
            strncpy(fm->dlg_buf, e->name, 63);
            fm->dlg_buf[63] = '\0';
            fm->dlg_cur = (int)strlen(fm->dlg_buf);
        }
        break;
    case ACT_DELETE:
        if (fm->sel_count > 0) {
            fm->dlg = DLG_DELETE;
            strcpy(fm->dlg_title, "Delete");
            if (fm->sel_count == 1 && fm->selected >= 0) {
                const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
                strcpy(fm->dlg_msg, "Delete \"");
                /* Safe append: dlg_msg is 96 bytes, we used 8 so far */
                {
                    char *p = fm->dlg_msg + strlen(fm->dlg_msg);
                    int remain = 90 - (int)(p - fm->dlg_msg);
                    int nlen = (int)strlen(e->name);
                    if (nlen > remain - 3) nlen = remain - 3;
                    memcpy(p, e->name, nlen);
                    p[nlen] = '\0';
                }
                strcat(fm->dlg_msg, "\"?");
            } else {
                char num[8];
                itoa(fm->sel_count, num, 10);
                strcpy(fm->dlg_msg, "Delete ");
                strcat(fm->dlg_msg, num);
                strcat(fm->dlg_msg, " items?");
            }
        }
        break;
    case ACT_REFRESH:
        fm_load(fm);
        break;
    case ACT_HIDDEN:
        fm->show_hidden = !fm->show_hidden;
        rebuild_view(fm);
        break;
    case ACT_CUT:
        fm_clip_copy(fm, 1);
        break;
    case ACT_COPY:
        fm_clip_copy(fm, 0);
        break;
    case ACT_PASTE:
        fm_clip_paste(fm);
        break;
    case ACT_SELECTALL:
        for (int i = 0; i < fm->view_count; i++) {
            if (strcmp(fm->entries[fm->view[i]].name, "..") != 0) {
                fm->sel_flags[i] = 1;
            }
        }
        fm->sel_count = 0;
        for (int i = 0; i < fm->view_count; i++)
            if (fm->sel_flags[i]) fm->sel_count++;
        show_toast(fm, "Selected all");
        break;
    case ACT_PROPS:
        if (fm->selected >= 0) {
            fm->dlg = DLG_PROPS;
            strcpy(fm->dlg_title, "Properties");
        }
        break;
    case ACT_GRIDVIEW:
        fm->view_mode = (fm->view_mode == VIEW_LIST) ? VIEW_GRID : VIEW_LIST;
        fm->scroll = 0;
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
            show_toast(fm, "File created");
            fm->dlg = DLG_NONE;
        } else {
            fm->dlg = DLG_MSG;
            strcpy(fm->dlg_title, "Error");
            strcpy(fm->dlg_msg, "Could not create file");
        }
        return;
    }
    if (fm->dlg == DLG_NEWFOLDER) {
        char path[256];
        build_path(fm->cwd, fm->dlg_buf, path);
        if (hal_fs_mkdir(path) == 0) {
            fm_load(fm);
            show_toast(fm, "Folder created");
            fm->dlg = DLG_NONE;
        } else {
            fm->dlg = DLG_MSG;
            strcpy(fm->dlg_title, "Error");
            strcpy(fm->dlg_msg, "Could not create folder");
        }
        return;
    }
    if (fm->dlg == DLG_RENAME) {
        if (fm->selected >= 0) {
            const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
            char old_path[256], new_path[256];
            build_path(fm->cwd, e->name, old_path);
            build_path(fm->cwd, fm->dlg_buf, new_path);
            if (hal_fs_rename(old_path, new_path) == 0) {
                fm_load(fm);
                show_toast(fm, "Renamed");
                fm->dlg = DLG_NONE;
            } else {
                fm->dlg = DLG_MSG;
                strcpy(fm->dlg_title, "Error");
                strcpy(fm->dlg_msg, "Rename failed");
            }
        }
        return;
    }
    if (fm->dlg == DLG_DELETE) {
        int deleted = 0;
        for (int i = 0; i < fm->view_count; i++) {
            if (fm->sel_flags[i]) {
                const fs_dirent_t *e = &fm->entries[fm->view[i]];
                if (strcmp(e->name, "..") == 0) continue;
                char path[256];
                build_path(fm->cwd, e->name, path);
                if (hal_fs_unlink(path) == 0) deleted++;
            }
        }
        char msg[64];
        strcpy(msg, "Deleted ");
        char num[8];
        itoa(deleted, num, 10);
        strcat(msg, num);
        strcat(msg, " item(s)");
        show_toast(fm, msg);
        fm_load(fm);
        fm->dlg = DLG_NONE;
        return;
    }
    fm->dlg = DLG_NONE;
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — toolbar
 *────────────────────────────────────────────────────────────────*/
static void r_toolbar(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    draw_filled_rect_blend(RECT(cr.x, cr.y, cr.w, TOOLBAR_H), tc->panel_bg);
    draw_line(cr.x, cr.y + TOOLBAR_H - 1,
              cr.x + cr.w, cr.y + TOOLBAR_H - 1, tc->panel_border);

    int bx = cr.x + 4, by = cr.y + 3, bs = 20;

    /* Navigation buttons */
    struct { const char *sym; int enabled; } nav[] = {
        { "<", fm->hist_i > 0 },
        { ">", fm->hist_i < fm->hist_n - 1 },
        { "^", strcmp(fm->cwd, "/") != 0 },
    };
    for (int i = 0; i < 3; i++) {
        Rect rb = RECT(bx, by, bs, bs);
        if (rect_contains(rb, mx, my) && nav[i].enabled)
            draw_filled_rounded_rect_blend(rb, 3, COLOR(255, 255, 255, 25));
        Color c = nav[i].enabled ? tc->text_primary : tc->text_muted;
        draw_text_centered(bx + bs / 2, by + 5, nav[i].sym, c, FONT_SIZE_SMALL);
        bx += bs + 2;
    }

    /* Separator */
    bx += 2;
    draw_line(bx, by + 3, bx, by + bs - 3, tc->panel_border);
    bx += 6;

    /* Breadcrumb path area */
    int pw = cr.x + cr.w - bx - 6;
    draw_filled_rounded_rect_blend(RECT(bx, by, pw, bs), 3, COLOR(0, 0, 0, 40));

    /* Home icon */
    icon_draw(ICON_HOME, bx + 4, by + 4, 12, tc->accent);
    int tx = bx + 20;

    /* Path segments */
    if (strcmp(fm->cwd, "/") != 0) {
        char tmp[256];
        strncpy(tmp, fm->cwd, 255);
        tmp[255] = '\0';
        char *p = tmp + 1;
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

    /* Filter indicator */
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

    int y = sy + 6;
    draw_text(sx + 10, y, "PLACES", tc->text_muted, FONT_SIZE_SMALL);
    y += 16;

    /* Quick-access items */
    struct { const char *label; const char *path; IconId icon; } qa[] = {
        { "Root",     "/",      ICON_HOME   },
        { "docs",     "/docs",  ICON_FOLDER },
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
    y += 4;
    widget_separator(sx + 8, y, sw - 16);
    y += 8;

    /* Clipboard info */
    if (fm->clip_count > 0) {
        draw_text(sx + 10, y, "CLIPBOARD", tc->text_muted, FONT_SIZE_SMALL);
        y += 14;
        char buf[32];
        itoa(fm->clip_count, buf, 10);
        strcat(buf, fm->clip_cut ? " cut" : " copied");
        draw_text(sx + 10, y + 2, buf, tc->text_secondary, FONT_SIZE_SMALL);
        y += 18;
        widget_separator(sx + 8, y, sw - 16);
        y += 8;
    }

    /* Disk info */
    draw_text(sx + 10, y, "DISK", tc->text_muted, FONT_SIZE_SMALL);
    y += 14;
    icon_draw(ICON_FILES, sx + 10, y + 2, 12, tc->text_secondary);
    draw_text(sx + 26, y + 3, "disk0", tc->text_secondary, FONT_SIZE_SMALL);
    y += 16;
    draw_text(sx + 26, y, "8 MB FAT16", tc->text_muted, FONT_SIZE_SMALL);
    y += 16;

    /* Free space bar */
    Rect bar = RECT(sx + 10, y, sw - 20, 6);
    draw_filled_rounded_rect(bar, 3, tc->panel_border);
    /* Approx 50% used just for visual */
    draw_filled_rounded_rect(RECT(bar.x, bar.y, bar.w / 2, bar.h), 3, tc->accent);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — column headers
 *────────────────────────────────────────────────────────────────*/
static int list_area_w(const FMData *fm)
{
    int w = fm->cw - SIDEBAR_W;
    if (fm->show_preview) w -= PREVIEW_W;
    return w;
}

static void r_colhdr(FMData *fm, Rect cr, const ThemeColors *tc)
{
    int x = cr.x + SIDEBAR_W;
    int y = cr.y + TOOLBAR_H;
    int w = list_area_w(fm);

    draw_filled_rect_blend(RECT(x, y, w, COLHDR_H), COLOR(0, 0, 0, 30));
    draw_line(x, y + COLHDR_H - 1, x + w, y + COLHDR_H - 1, tc->panel_border);

    int nw = w * 55 / 100;
    int sw = w * 20 / 100;
    Color c;

    c = (fm->sort_col == SORT_NAME) ? tc->accent : tc->text_muted;
    draw_text(x + 22, y + 3, "Name", c, FONT_SIZE_SMALL);
    if (fm->sort_col == SORT_NAME)
        draw_text(x + 50, y + 3, fm->sort_asc ? "v" : "^", tc->accent, FONT_SIZE_SMALL);

    c = (fm->sort_col == SORT_SIZE) ? tc->accent : tc->text_muted;
    draw_text(x + nw + 4, y + 3, "Size", c, FONT_SIZE_SMALL);
    if (fm->sort_col == SORT_SIZE)
        draw_text(x + nw + 30, y + 3, fm->sort_asc ? "v" : "^", tc->accent, FONT_SIZE_SMALL);

    c = (fm->sort_col == SORT_TYPE) ? tc->accent : tc->text_muted;
    draw_text(x + nw + sw + 4, y + 3, "Type", c, FONT_SIZE_SMALL);
    if (fm->sort_col == SORT_TYPE)
        draw_text(x + nw + sw + 30, y + 3, fm->sort_asc ? "v" : "^", tc->accent, FONT_SIZE_SMALL);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — file list (list view)
 *────────────────────────────────────────────────────────────────*/
static void r_filelist(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int x = cr.x + SIDEBAR_W;
    int y = cr.y + TOOLBAR_H + COLHDR_H;
    int w = list_area_w(fm);
    int h = cr.h - TOOLBAR_H - COLHDR_H - STATUSBAR_H;
    int vis = h / ROW_H;
    int nw = w * 55 / 100;
    int sw = w * 20 / 100;

    for (int vi = fm->scroll; vi < fm->view_count && vi < fm->scroll + vis; vi++) {
        int ry = y + (vi - fm->scroll) * ROW_H;
        Rect row = RECT(x, ry, w, ROW_H);
        int hover = rect_contains(row, mx, my);
        int sel = fm->sel_flags[vi];
        const fs_dirent_t *e = &fm->entries[fm->view[vi]];
        int is_dir = (e->type == FS_TYPE_DIR);
        int is_up = (strcmp(e->name, "..") == 0);

        /* Check if this item is in clip_cut (show dimmed) */
        int is_cut = 0;
        if (fm->clip_cut && fm->clip_count > 0) {
            char path[256];
            build_path(fm->cwd, e->name, path);
            for (int c = 0; c < fm->clip_count; c++) {
                if (strcmp(fm->clip_paths[c], path) == 0) { is_cut = 1; break; }
            }
        }

        /* Row background */
        if (sel)
            draw_filled_rect_blend(row, tc->accent_dim);
        else if (hover)
            draw_filled_rect_blend(row, COLOR(255, 255, 255, 12));

        /* Icon */
        Color ic = is_up ? tc->text_muted :
                   is_dir ? tc->accent : tc->text_secondary;
        if (is_cut) ic = COLOR(ic.r, ic.g, ic.b, 100);
        {
            int fimg = !is_dir && is_image_file(e->name);
            IconId fid = is_dir ? ICON_FOLDER : (fimg ? ICON_IMAGE : ICON_FILE);
            icon_draw(fid, x + 4, ry + 3, 14, ic);
        }

        /* Name */
        Color nc = sel ? COLOR_WHITE : tc->text_primary;
        if (is_cut) nc = COLOR(nc.r, nc.g, nc.b, 100);
        draw_text(x + 22, ry + 5, e->name, nc, FONT_SIZE_SMALL);

        /* Size */
        if (!is_dir) {
            char sz[24];
            fmt_size(e->size, sz);
            draw_text(x + nw + 4, ry + 5, sz, tc->text_muted, FONT_SIZE_SMALL);
        }

        /* Type */
        const char *type = is_up ? "Parent" : ftype_str(e);
        draw_text(x + nw + sw + 4, ry + 5, type, tc->text_muted, FONT_SIZE_SMALL);

        /* Row separator */
        if (vi < fm->scroll + vis - 1)
            draw_line(x + 4, ry + ROW_H - 1, x + w - 4,
                      ry + ROW_H - 1, COLOR(255, 255, 255, 8));
    }

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
 * Rendering — grid view
 *────────────────────────────────────────────────────────────────*/
static void r_gridview(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int x = cr.x + SIDEBAR_W;
    int y = cr.y + TOOLBAR_H + COLHDR_H;
    int w = list_area_w(fm);
    int h = cr.h - TOOLBAR_H - COLHDR_H - STATUSBAR_H;

    int cols = w / GRID_CELL_W;
    if (cols < 1) cols = 1;
    int pad = 8;

    int row_start = fm->scroll;
    int vis_rows = h / GRID_CELL_H + 1;

    for (int vi = row_start; vi < fm->view_count; vi++) {
        int grid_row = (vi - row_start) / cols;
        int grid_col = (vi - row_start) % cols;
        if (grid_row >= vis_rows) break;

        int cx = x + pad + grid_col * GRID_CELL_W;
        int cy = y + pad + grid_row * GRID_CELL_H;

        if (cy + GRID_CELL_H > y + h) break;

        const fs_dirent_t *e = &fm->entries[fm->view[vi]];
        int is_dir = (e->type == FS_TYPE_DIR);
        int sel = fm->sel_flags[vi];

        Rect cell = RECT(cx, cy, GRID_CELL_W - 4, GRID_CELL_H - 4);
        int hover = rect_contains(cell, mx, my);

        if (sel)
            draw_filled_rounded_rect_blend(cell, 4, tc->accent_dim);
        else if (hover)
            draw_filled_rounded_rect_blend(cell, 4, COLOR(255, 255, 255, 15));

        /* Large icon */
        Color ic = is_dir ? tc->accent : tc->text_secondary;
        {
            int gimg = !is_dir && is_image_file(e->name);
            IconId gid = is_dir ? ICON_FOLDER : (gimg ? ICON_IMAGE : ICON_FILE);
            icon_draw(gid, cx + (GRID_CELL_W - 4) / 2 - 12, cy + 8, 24, ic);
        }

        /* Name (truncated) */
        char name[12];
        strncpy(name, e->name, 11);
        name[11] = '\0';
        if (strlen(e->name) > 11) {
            name[9] = '.';
            name[10] = '.';
        }
        Color nc = sel ? COLOR_WHITE : tc->text_primary;
        draw_text_centered(cx + (GRID_CELL_W - 4) / 2, cy + 42,
                          name, nc, FONT_SIZE_SMALL);
    }

    if (fm->view_count == 0)
        draw_text_centered(x + w / 2, y + h / 2 - 5,
                          "Empty folder", tc->text_muted, FONT_SIZE_SMALL);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — preview panel
 *────────────────────────────────────────────────────────────────*/
static void r_preview(FMData *fm, Rect cr, const ThemeColors *tc)
{
    if (!fm->show_preview) return;

    int px = cr.x + cr.w - PREVIEW_W;
    int py = cr.y + TOOLBAR_H;
    int pw = PREVIEW_W;
    int ph = cr.h - TOOLBAR_H - STATUSBAR_H;

    draw_filled_rect_blend(RECT(px, py, pw, ph), tc->sidebar_bg);
    draw_line(px, py, px, py + ph, tc->panel_border);

    int y = py + 10;

    if (fm->selected >= 0 && fm->selected < fm->view_count) {
        const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
        int is_dir = (e->type == FS_TYPE_DIR);

        /* Icon */
        int is_img = !is_dir && is_image_file(e->name);
        IconId iid = is_dir ? ICON_FOLDER : (is_img ? ICON_IMAGE : ICON_FILE);
        Color ic = is_dir ? tc->accent : tc->text_secondary;
        icon_draw(iid, px + pw / 2 - 16, y, 32, ic);
        y += 40;

        /* Name */
        draw_text_centered(px + pw / 2, y, e->name, tc->text_primary, FONT_SIZE_SMALL);
        y += 16;

        /* Type */
        draw_text_centered(px + pw / 2, y, ftype_str(e), tc->text_muted, FONT_SIZE_SMALL);
        y += 20;

        widget_separator(px + 10, y, pw - 20);
        y += 10;

        /* Details */
        draw_text(px + 10, y, "Size:", tc->text_muted, FONT_SIZE_SMALL);
        if (!is_dir) {
            char sz[24];
            fmt_size(e->size, sz);
            draw_text_right(px + pw - 10, y, sz, tc->text_secondary, FONT_SIZE_SMALL);
        } else {
            draw_text_right(px + pw - 10, y, "--", tc->text_secondary, FONT_SIZE_SMALL);
        }
        y += 14;

        draw_text(px + 10, y, "Type:", tc->text_muted, FONT_SIZE_SMALL);
        draw_text_right(px + pw - 10, y, ftype_str(e), tc->text_secondary, FONT_SIZE_SMALL);
        y += 14;

        draw_text(px + 10, y, "Path:", tc->text_muted, FONT_SIZE_SMALL);
        y += 12;
        char path[256];
        build_path(fm->cwd, e->name, path);
        /* Truncate long paths */
        if (strlen(path) > 20) {
            path[18] = '.';
            path[19] = '.';
            path[20] = '\0';
        }
        draw_text(px + 10, y, path, tc->text_secondary, FONT_SIZE_SMALL);
        y += 20;

        /* Preview content for text files */
        if (!is_dir && e->size > 0 && e->size < 4096) {
            const char *dot = strrchr(e->name, '.');
            int is_text = (!dot || strcmp(dot, ".txt") == 0 ||
                          strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0 ||
                          strcmp(dot, ".s") == 0 || strcmp(dot, ".sh") == 0 ||
                          strcmp(dot, ".md") == 0 || strcmp(dot, ".cfg") == 0);
            if (is_text) {
                widget_separator(px + 10, y, pw - 20);
                y += 10;
                draw_text(px + 10, y, "Preview:", tc->text_muted, FONT_SIZE_SMALL);
                y += 14;

                /* Read file content */
                char preview[256];
                char full_path[256];
                build_path(fm->cwd, e->name, full_path);
                int fd = hal_fs_open(full_path, FS_O_READ);
                if (fd >= 0) {
                    int n = hal_fs_read(fd, preview, 240);
                    hal_fs_close(fd);
                    if (n > 0) {
                        preview[n] = '\0';
                        /* Show up to 6 lines */
                        char *line = preview;
                        int lines = 0;
                        while (line && *line && lines < 6 && y < py + ph - 10) {
                            char *nl = strchr(line, '\n');
                            if (nl) *nl = '\0';
                            /* Truncate long lines */
                            if (strlen(line) > 22) line[22] = '\0';
                            draw_text(px + 10, y, line,
                                     tc->text_secondary, FONT_SIZE_SMALL);
                            y += 11;
                            lines++;
                            if (nl) line = nl + 1;
                            else break;
                        }
                    }
                }
            }
        }

        /* Preview thumbnail for image files */
        if (is_img && e->size > 0) {
            widget_separator(px + 10, y, pw - 20);
            y += 10;

            /* Cache thumbnail — only decode when selection changes */
            if (fm->preview_for_idx != fm->selected) {
                hal_image_free(&fm->preview_thumb);
                fm->preview_for_idx = -1;

                char full_path[256];
                build_path(fm->cwd, e->name, full_path);
                hal_image_t full;
                if (hal_image_load(full_path, &full) == 0) {
                    /* Scale to fit ~140x100 */
                    int tw = 140, th = 100;
                    int sw = tw * 1000 / full.width;
                    int sh = th * 1000 / full.height;
                    int s = sw < sh ? sw : sh;
                    if (s > 1000) s = 1000;
                    int nw = full.width * s / 1000;
                    int nh = full.height * s / 1000;
                    if (nw < 1) nw = 1;
                    if (nh < 1) nh = 1;
                    if (nw == full.width && nh == full.height) {
                        fm->preview_thumb = full;
                    } else {
                        hal_image_scale(&full, nw, nh, &fm->preview_thumb);
                        hal_image_free(&full);
                    }
                    fm->preview_for_idx = fm->selected;
                }
            }

            if (fm->preview_thumb.pixels) {
                int tx = px + (pw - fm->preview_thumb.width) / 2;
                hal_fb_blit(tx, y, fm->preview_thumb.width,
                           fm->preview_thumb.height,
                           fm->preview_thumb.pixels,
                           fm->preview_thumb.width * 4);
                y += fm->preview_thumb.height + 4;

                /* Show dimensions */
                char dbuf[32], wbuf[12], hbuf[12];
                hal_image_t full_tmp;
                build_path(fm->cwd, e->name, dbuf);
                /* Use cached original size — re-stat */
                itoa((int)e->size, wbuf, 10);
                strcpy(dbuf, wbuf);
                strcat(dbuf, " B");
                draw_text_centered(px + pw / 2, y, dbuf,
                                  tc->text_muted, FONT_SIZE_SMALL);
                (void)full_tmp; (void)hbuf;
            }
        }
    } else if (fm->sel_count > 1) {
        /* Multiple selection */
        icon_draw(ICON_FILES, px + pw / 2 - 16, y, 32, tc->text_secondary);
        y += 40;
        char buf[32];
        itoa(fm->sel_count, buf, 10);
        strcat(buf, " selected");
        draw_text_centered(px + pw / 2, y, buf, tc->text_primary, FONT_SIZE_SMALL);
    } else {
        /* No selection — show folder info */
        icon_draw(ICON_FOLDER, px + pw / 2 - 16, y, 32, tc->accent);
        y += 40;
        draw_text_centered(px + pw / 2, y, fm->cwd, tc->text_primary, FONT_SIZE_SMALL);
        y += 16;
        char buf[32];
        itoa(fm->view_count, buf, 10);
        strcat(buf, " items");
        draw_text_centered(px + pw / 2, y, buf, tc->text_muted, FONT_SIZE_SMALL);
    }
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — status bar
 *────────────────────────────────────────────────────────────────*/
static void r_status(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int y = cr.y + cr.h - STATUSBAR_H;

    draw_filled_rect_blend(RECT(cr.x, y, cr.w, STATUSBAR_H), tc->panel_bg);
    draw_line(cr.x, y, cr.x + cr.w, y, tc->panel_border);

    /* Item count (left) */
    char buf[48];
    itoa(fm->view_count, buf, 10);
    strcat(buf, " items");
    if (fm->sel_count > 0) {
        strcat(buf, " (");
        char num[8];
        itoa(fm->sel_count, num, 10);
        strcat(buf, num);
        strcat(buf, " selected)");
    }
    draw_text(cr.x + 8, y + 5, buf, tc->text_muted, FONT_SIZE_SMALL);

    /* View toggle buttons (right side) */
    int bx = cr.x + cr.w - 60;
    int by2 = y + 3;

    /* List view button */
    Rect lb = RECT(bx, by2, 22, 14);
    int list_active = (fm->view_mode == VIEW_LIST);
    if (list_active)
        draw_filled_rounded_rect_blend(lb, 2, tc->accent_dim);
    else if (rect_contains(lb, mx, my))
        draw_filled_rounded_rect_blend(lb, 2, COLOR(255, 255, 255, 20));
    draw_text_centered(bx + 11, by2 + 2, "=", list_active ? tc->accent : tc->text_muted, FONT_SIZE_SMALL);

    /* Grid view button */
    bx += 26;
    Rect gb = RECT(bx, by2, 22, 14);
    int grid_active = (fm->view_mode == VIEW_GRID);
    if (grid_active)
        draw_filled_rounded_rect_blend(gb, 2, tc->accent_dim);
    else if (rect_contains(gb, mx, my))
        draw_filled_rounded_rect_blend(gb, 2, COLOR(255, 255, 255, 20));
    draw_text_centered(bx + 11, by2 + 2, "#", grid_active ? tc->accent : tc->text_muted, FONT_SIZE_SMALL);

    /* Preview toggle (before view buttons) */
    bx = cr.x + cr.w - 90;
    Rect pb = RECT(bx, by2, 22, 14);
    if (fm->show_preview)
        draw_filled_rounded_rect_blend(pb, 2, tc->accent_dim);
    else if (rect_contains(pb, mx, my))
        draw_filled_rounded_rect_blend(pb, 2, COLOR(255, 255, 255, 20));
    icon_draw(ICON_SEARCH, bx + 5, by2 + 1, 12,
              fm->show_preview ? tc->accent : tc->text_muted);
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — context menu
 *────────────────────────────────────────────────────────────────*/
static void r_ctx(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    int mh = ctx_height(fm);

    int ax = cr.x + fm->ctx_x;
    int ay = cr.y + fm->ctx_y;
    if (ax + CTX_W > cr.x + cr.w) ax = cr.x + cr.w - CTX_W;
    if (ay + mh > cr.y + cr.h)    ay = cr.y + cr.h - mh;

    /* Shadow + background */
    draw_filled_rounded_rect_blend(RECT(ax + 2, ay + 2, CTX_W, mh), 5,
                                    COLOR(0, 0, 0, 100));
    draw_filled_rounded_rect(RECT(ax, ay, CTX_W, mh), 4, tc->window_bg);
    draw_rounded_rect(RECT(ax, ay, CTX_W, mh), 4, tc->panel_border);

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

        Color c;
        if (!fm->ctx_items[i].enabled)
            c = tc->text_muted;
        else if (fm->ctx_items[i].danger)
            c = hov ? COLOR(255, 150, 150, 255) : tc->error;
        else
            c = hov ? COLOR_WHITE : tc->text_primary;

        draw_text(ax + 12, iy + 5, fm->ctx_items[i].label, c, FONT_SIZE_SMALL);

        /* Shortcut on right */
        if (fm->ctx_items[i].shortcut && fm->ctx_items[i].shortcut[0]) {
            draw_text_right(ax + CTX_W - 12, iy + 5,
                           fm->ctx_items[i].shortcut, tc->text_muted, FONT_SIZE_SMALL);
        }

        iy += CTX_ITEM_H;
    }
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — dialogs
 *────────────────────────────────────────────────────────────────*/
static void r_dialog(FMData *fm, Rect cr, const ThemeColors *tc, int mx, int my)
{
    /* Dim overlay */
    draw_filled_rect_blend(cr, COLOR(0, 0, 0, 120));

    int dw, dh;
    if (fm->dlg == DLG_PROPS) {
        dw = 240; dh = 160;
    } else if (fm->dlg == DLG_DELETE) {
        dw = 250; dh = 110;
    } else if (fm->dlg == DLG_MSG) {
        dw = 220; dh = 85;
    } else {
        dw = 240; dh = 105;
    }

    int dx = cr.x + (cr.w - dw) / 2;
    int dy = cr.y + (cr.h - dh) / 2;

    /* Dialog frame */
    draw_filled_rounded_rect_blend(RECT(dx + 3, dy + 3, dw, dh), 6,
                                    COLOR(0, 0, 0, 80));
    draw_filled_rounded_rect(RECT(dx, dy, dw, dh), 6, tc->window_bg);
    draw_rounded_rect(RECT(dx, dy, dw, dh), 6, tc->accent_dim);

    /* Title */
    draw_text(dx + 12, dy + 10, fm->dlg_title, tc->text_primary, FONT_SIZE_SMALL);
    draw_line(dx + 8, dy + 24, dx + dw - 8, dy + 24, tc->panel_border);

    if (fm->dlg == DLG_MSG) {
        draw_text(dx + 12, dy + 34, fm->dlg_msg, tc->text_secondary, FONT_SIZE_SMALL);
        Rect ok = RECT(dx + dw - 60, dy + dh - 30, 48, 22);
        widget_button(ok, "OK", BTN_PRIMARY, mx, my, 0);

    } else if (fm->dlg == DLG_DELETE) {
        draw_text(dx + 12, dy + 32, fm->dlg_msg, tc->text_secondary, FONT_SIZE_SMALL);
        draw_text(dx + 12, dy + 52, "This cannot be undone.", tc->error, FONT_SIZE_SMALL);
        Rect cancel = RECT(dx + dw - 140, dy + dh - 30, 60, 22);
        Rect del = RECT(dx + dw - 70, dy + dh - 30, 58, 22);
        widget_button(cancel, "Cancel", BTN_SECONDARY, mx, my, 0);
        widget_button(del, "Delete", BTN_DANGER, mx, my, 0);

    } else if (fm->dlg == DLG_PROPS) {
        if (fm->selected >= 0 && fm->selected < fm->view_count) {
            const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
            int y = dy + 30;

            draw_text(dx + 12, y, "Name:", tc->text_muted, FONT_SIZE_SMALL);
            draw_text(dx + 60, y, e->name, tc->text_primary, FONT_SIZE_SMALL);
            y += 14;

            draw_text(dx + 12, y, "Type:", tc->text_muted, FONT_SIZE_SMALL);
            draw_text(dx + 60, y, ftype_str(e), tc->text_primary, FONT_SIZE_SMALL);
            y += 14;

            draw_text(dx + 12, y, "Size:", tc->text_muted, FONT_SIZE_SMALL);
            char sz[24];
            fmt_size(e->size, sz);
            draw_text(dx + 60, y, sz, tc->text_primary, FONT_SIZE_SMALL);
            y += 14;

            draw_text(dx + 12, y, "Path:", tc->text_muted, FONT_SIZE_SMALL);
            char path[256];
            build_path(fm->cwd, e->name, path);
            if (strlen(path) > 26) { path[24] = '.'; path[25] = '.'; path[26] = '\0'; }
            draw_text(dx + 60, y, path, tc->text_primary, FONT_SIZE_SMALL);
            y += 14;

            draw_text(dx + 12, y, "Attr:", tc->text_muted, FONT_SIZE_SMALL);
            draw_text(dx + 60, y, e->type == FS_TYPE_DIR ? "Directory" : "Regular file",
                     tc->text_primary, FONT_SIZE_SMALL);
            y += 14;

            draw_text(dx + 12, y, "FS:", tc->text_muted, FONT_SIZE_SMALL);
            draw_text(dx + 60, y, "FAT16", tc->text_primary, FONT_SIZE_SMALL);
        }
        Rect ok = RECT(dx + dw - 60, dy + dh - 30, 48, 22);
        widget_button(ok, "OK", BTN_PRIMARY, mx, my, 0);

    } else {
        /* Text input dialogs (new file, new folder, rename) */
        Rect inp = RECT(dx + 12, dy + 32, dw - 24, 22);
        draw_filled_rounded_rect(inp, 3, COLOR(0, 0, 0, 60));
        draw_rounded_rect(inp, 3, tc->accent);
        draw_text(dx + 16, dy + 38, fm->dlg_buf, tc->text_primary, FONT_SIZE_SMALL);

        /* Blinking cursor */
        if ((hal_get_ticks() / 500) % 2 == 0) {
            int cx = dx + 16 + fm->dlg_cur * 6;
            draw_filled_rect(RECT(cx, dy + 36, 1, 12), tc->accent);
        }

        const char *ok_label = "Create";
        if (fm->dlg == DLG_RENAME) ok_label = "Rename";

        Rect cancel = RECT(dx + dw - 130, dy + dh - 30, 58, 22);
        Rect ok = RECT(dx + dw - 65, dy + dh - 30, 53, 22);
        widget_button(cancel, "Cancel", BTN_SECONDARY, mx, my, 0);
        widget_button(ok, ok_label, BTN_PRIMARY, mx, my, 0);
    }
}

/*──────────────────────────────────────────────────────────────────
 * Rendering — toast notification
 *────────────────────────────────────────────────────────────────*/
static void r_toast(FMData *fm, Rect cr, const ThemeColors *tc)
{
    if (fm->toast_msg[0] == '\0') return;
    uint32_t elapsed = hal_get_ticks() - fm->toast_time;
    if (elapsed > 2500) {
        fm->toast_msg[0] = '\0';
        return;
    }

    int tw = draw_text_width(fm->toast_msg, FONT_SIZE_SMALL) + 20;
    int tx = cr.x + cr.w - tw - 10;
    int ty = cr.y + cr.h - STATUSBAR_H - 30;

    /* Fade out in last 500ms */
    int alpha = 220;
    if (elapsed > 2000)
        alpha = 220 * (int)(2500 - elapsed) / 500;

    draw_filled_rounded_rect_blend(RECT(tx, ty, tw, 22), 4,
                                    COLOR(40, 40, 60, (uint8_t)alpha));
    draw_rounded_rect(RECT(tx, ty, tw, 22), 4,
                     COLOR(tc->accent.r, tc->accent.g, tc->accent.b, (uint8_t)alpha));
    Color tcolor = COLOR(tc->text_primary.r, tc->text_primary.g,
                         tc->text_primary.b, (uint8_t)alpha);
    draw_text(tx + 10, ty + 6, fm->toast_msg, tcolor, FONT_SIZE_SMALL);
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

    if (fm->view_mode == VIEW_LIST) {
        r_colhdr(fm, cr, tc);
        r_filelist(fm, cr, tc, mx, my);
    } else {
        r_gridview(fm, cr, tc, mx, my);
    }

    r_preview(fm, cr, tc);
    r_status(fm, cr, tc, mx, my);
    r_toast(fm, cr, tc);

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
        int dw, dh;
        if (fm->dlg == DLG_PROPS) { dw = 240; dh = 160; }
        else if (fm->dlg == DLG_DELETE) { dw = 250; dh = 110; }
        else if (fm->dlg == DLG_MSG) { dw = 220; dh = 85; }
        else { dw = 240; dh = 105; }
        int dx = (fm->cw - dw) / 2;
        int dy = (fm->ch - dh) / 2;

        if (fm->dlg == DLG_MSG || fm->dlg == DLG_PROPS) {
            /* OK button or anywhere */
            if (x >= dx + dw - 60 && x < dx + dw - 12 &&
                y >= dy + dh - 30 && y < dy + dh - 8) {
                fm->dlg = DLG_NONE;
            } else if (x < dx || x > dx + dw || y < dy || y > dy + dh) {
                fm->dlg = DLG_NONE;
            }
        } else if (fm->dlg == DLG_DELETE) {
            if (x >= dx + dw - 70 && x < dx + dw - 12 &&
                y >= dy + dh - 30 && y < dy + dh - 8) {
                dlg_confirm(fm);
            } else if (x >= dx + dw - 140 && x < dx + dw - 80 &&
                       y >= dy + dh - 30 && y < dy + dh - 8) {
                fm->dlg = DLG_NONE;
            } else if (x < dx || x > dx + dw || y < dy || y > dy + dh) {
                fm->dlg = DLG_NONE;
            }
        } else {
            /* Input dialogs: Cancel */
            if (x >= dx + dw - 130 && x < dx + dw - 72 &&
                y >= dy + dh - 30 && y < dy + dh - 8) {
                fm->dlg = DLG_NONE;
                return;
            }
            /* OK/Create/Rename */
            if (x >= dx + dw - 65 && x < dx + dw - 12 &&
                y >= dy + dh - 30 && y < dy + dh - 8) {
                dlg_confirm(fm);
                return;
            }
            /* Click outside = cancel */
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
        int lw = list_area_w(fm);
        int list_y0 = TOOLBAR_H + (fm->view_mode == VIEW_LIST ? COLHDR_H : 0);
        int list_h = fm->ch - list_y0 - STATUSBAR_H;

        if (x >= SIDEBAR_W && x < SIDEBAR_W + lw && y >= list_y0) {
            if (fm->view_mode == VIEW_LIST) {
                int vis = list_h / ROW_H;
                int row = (y - list_y0) / ROW_H + fm->scroll;
                if (row >= 0 && row < fm->view_count && row < fm->scroll + vis) {
                    if (!fm->sel_flags[row]) select_single(fm, row);
                    fm->ctx_target = row;
                    build_ctx_file(fm);
                } else {
                    fm->ctx_target = -1;
                    build_ctx_bg(fm);
                }
            } else {
                /* Grid view hit test */
                int cols = lw / GRID_CELL_W;
                if (cols < 1) cols = 1;
                int gx = (x - SIDEBAR_W - 8) / GRID_CELL_W;
                int gy = (y - list_y0 - 8) / GRID_CELL_H;
                int vi = fm->scroll + gy * cols + gx;
                if (vi >= 0 && vi < fm->view_count) {
                    if (!fm->sel_flags[vi]) select_single(fm, vi);
                    fm->ctx_target = vi;
                    build_ctx_file(fm);
                } else {
                    fm->ctx_target = -1;
                    build_ctx_bg(fm);
                }
            }
        } else {
            fm->ctx_target = -1;
            build_ctx_bg(fm);
        }

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
    uint16_t mod = 0;
    /* Check for modifier keys from HAL (we read raw state) */
    extern uint16_t hal_input_get_mod(void);
    mod = hal_input_get_mod();

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

    /* Status bar — view toggle buttons */
    if (y >= fm->ch - STATUSBAR_H) {
        int bx2 = fm->cw - 60;
        if (x >= bx2 && x < bx2 + 22) { fm->view_mode = VIEW_LIST; fm->scroll = 0; return; }
        bx2 += 26;
        if (x >= bx2 && x < bx2 + 22) { fm->view_mode = VIEW_GRID; fm->scroll = 0; return; }
        /* Preview toggle */
        bx2 = fm->cw - 90;
        if (x >= bx2 && x < bx2 + 22) { fm->show_preview = !fm->show_preview; return; }
        return;
    }

    /* Sidebar */
    if (x < SIDEBAR_W && y >= TOOLBAR_H) {
        int sy = TOOLBAR_H + 22;
        const char *paths[] = {"/", "/docs"};
        for (int i = 0; i < 2; i++) {
            if (y >= sy && y < sy + 22) {
                fm_navigate(fm, paths[i]);
                return;
            }
            sy += 22;
        }
        return;
    }

    /* Column headers (list view only) */
    if (fm->view_mode == VIEW_LIST &&
        y >= TOOLBAR_H && y < TOOLBAR_H + COLHDR_H && x >= SIDEBAR_W) {
        int lx = x - SIDEBAR_W;
        int w = list_area_w(fm);
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

    /* File list / grid */
    int lw = list_area_w(fm);
    int list_y0 = TOOLBAR_H + (fm->view_mode == VIEW_LIST ? COLHDR_H : 0);
    int list_h = fm->ch - list_y0 - STATUSBAR_H;

    if (y >= list_y0 && y < list_y0 + list_h && x >= SIDEBAR_W && x < SIDEBAR_W + lw) {
        int vi = -1;
        if (fm->view_mode == VIEW_LIST) {
            vi = (y - list_y0) / ROW_H + fm->scroll;
            if (vi >= fm->view_count) vi = -1;
        } else {
            int cols = lw / GRID_CELL_W;
            if (cols < 1) cols = 1;
            int gx = (x - SIDEBAR_W - 8) / GRID_CELL_W;
            int gy = (y - list_y0 - 8) / GRID_CELL_H;
            vi = fm->scroll + gy * cols + gx;
            if (vi >= fm->view_count) vi = -1;
        }

        if (vi >= 0) {
            /* Double-click detection */
            uint32_t now = hal_get_ticks();
            if (vi == fm->dbl_item && (now - fm->dbl_time) < DBLCLICK_MS) {
                select_single(fm, vi);
                fm_open_selected(fm);
                fm->dbl_item = -1;
                return;
            }
            fm->dbl_time = now;
            fm->dbl_item = vi;

            /* Selection with modifiers */
            if (mod & HAL_MOD_CTRL) {
                toggle_select(fm, vi);
            } else if (mod & HAL_MOD_SHIFT) {
                range_select(fm, vi);
            } else {
                select_single(fm, vi);
            }
        } else {
            clear_selection(fm);
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
            if (fm->dlg == DLG_MSG || fm->dlg == DLG_PROPS)
                fm->dlg = DLG_NONE;
            else
                dlg_confirm(fm);
            return;
        }
        /* Text editing */
        if (fm->dlg != DLG_MSG && fm->dlg != DLG_PROPS && fm->dlg != DLG_DELETE) {
            if (key == HAL_KEY_BACKSPACE && fm->dlg_cur > 0) {
                int len = (int)strlen(fm->dlg_buf);
                memmove(fm->dlg_buf + fm->dlg_cur - 1,
                        fm->dlg_buf + fm->dlg_cur,
                        (unsigned)(len - fm->dlg_cur + 1));
                fm->dlg_cur--;
                return;
            }
            if (key == HAL_KEY_LEFT && fm->dlg_cur > 0) { fm->dlg_cur--; return; }
            if (key == HAL_KEY_RIGHT && fm->dlg_cur < (int)strlen(fm->dlg_buf)) { fm->dlg_cur++; return; }
            if (key == HAL_KEY_HOME) { fm->dlg_cur = 0; return; }
            if (key == HAL_KEY_END) { fm->dlg_cur = (int)strlen(fm->dlg_buf); return; }
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

    /* Ctrl+P = toggle preview */
    if (key == HAL_KEY_P && (mod & HAL_MOD_CTRL)) {
        fm->show_preview = !fm->show_preview;
        return;
    }

    /* Ctrl+N = new file */
    if (key == HAL_KEY_N && (mod & HAL_MOD_CTRL) && !(mod & HAL_MOD_SHIFT)) {
        fm->dlg = DLG_NEWFILE;
        strcpy(fm->dlg_title, "New File");
        strcpy(fm->dlg_buf, "NEWFILE.TXT");
        fm->dlg_cur = 7;
        return;
    }

    /* Ctrl+Shift+N = new folder */
    if (key == HAL_KEY_N && (mod & HAL_MOD_CTRL) && (mod & HAL_MOD_SHIFT)) {
        fm->dlg = DLG_NEWFOLDER;
        strcpy(fm->dlg_title, "New Folder");
        strcpy(fm->dlg_buf, "NEWFOLDER");
        fm->dlg_cur = 9;
        return;
    }

    /* Ctrl+A = select all */
    if (key == HAL_KEY_A && (mod & HAL_MOD_CTRL)) {
        ctx_exec(fm, ACT_SELECTALL);
        return;
    }

    /* Ctrl+C = copy */
    if (key == HAL_KEY_C && (mod & HAL_MOD_CTRL)) {
        if (fm->sel_count > 0) fm_clip_copy(fm, 0);
        return;
    }

    /* Ctrl+X = cut */
    if (key == HAL_KEY_X && (mod & HAL_MOD_CTRL)) {
        if (fm->sel_count > 0) fm_clip_copy(fm, 1);
        return;
    }

    /* Ctrl+V = paste */
    if (key == HAL_KEY_V && (mod & HAL_MOD_CTRL)) {
        fm_clip_paste(fm);
        return;
    }

    /* F2 = rename */
    if (key == HAL_KEY_F2 && fm->selected >= 0) {
        const fs_dirent_t *e = &fm->entries[fm->view[fm->selected]];
        if (strcmp(e->name, "..") != 0) {
            fm->dlg = DLG_RENAME;
            strcpy(fm->dlg_title, "Rename");
            strncpy(fm->dlg_buf, e->name, 63);
            fm->dlg_buf[63] = '\0';
            fm->dlg_cur = (int)strlen(fm->dlg_buf);
        }
        return;
    }

    /* Delete key */
    if (key == HAL_KEY_DELETE && fm->sel_count > 0) {
        ctx_exec(fm, ACT_DELETE);
        return;
    }

    /* Escape = clear filter or deselect */
    if (key == HAL_KEY_ESCAPE) {
        if (fm->filter_len > 0) {
            fm->filter_len = 0;
            fm->filter[0] = '\0';
            rebuild_view(fm);
        } else {
            clear_selection(fm);
        }
        return;
    }

    /* ── Arrow key navigation ─────────────────────────────────── */
    int vis;
    if (fm->view_mode == VIEW_LIST) {
        vis = (fm->ch - TOOLBAR_H - COLHDR_H - STATUSBAR_H) / ROW_H;
    } else {
        vis = fm->view_count; /* simplified for grid */
    }
    if (vis <= 0) vis = 1;

    if (key == HAL_KEY_UP) {
        int next = fm->selected > 0 ? fm->selected - 1 : 0;
        if (mod & HAL_MOD_SHIFT)
            range_select(fm, next);
        else
            select_single(fm, next);
        if (fm->selected < fm->scroll) fm->scroll = fm->selected;
        return;
    }
    if (key == HAL_KEY_DOWN) {
        int next = fm->selected < fm->view_count - 1 ? fm->selected + 1 : fm->view_count - 1;
        if (next < 0) next = 0;
        if (mod & HAL_MOD_SHIFT)
            range_select(fm, next);
        else
            select_single(fm, next);
        if (fm->view_mode == VIEW_LIST && fm->selected >= fm->scroll + vis)
            fm->scroll = fm->selected - vis + 1;
        return;
    }
    if (key == HAL_KEY_HOME) { select_single(fm, 0); fm->scroll = 0; return; }
    if (key == HAL_KEY_END) {
        select_single(fm, fm->view_count - 1);
        if (fm->view_count - 1 >= vis)
            fm->scroll = fm->view_count - vis;
        return;
    }
    if (key == HAL_KEY_PAGEUP) {
        int next = max_i(0, fm->selected - vis);
        select_single(fm, next);
        fm->scroll = max_i(0, fm->scroll - vis);
        return;
    }
    if (key == HAL_KEY_PAGEDOWN) {
        int next = min_i(fm->view_count - 1, fm->selected + vis);
        select_single(fm, next);
        if (fm->selected >= fm->scroll + vis)
            fm->scroll = fm->selected - vis + 1;
        return;
    }
}

/*──────────────────────────────────────────────────────────────────
 * Text input handler
 *────────────────────────────────────────────────────────────────*/
static void fm_on_text(AppContent *self, char ch)
{
    FMData *fm = (FMData *)self->data;

    /* Dialog text input */
    if (fm->dlg != DLG_NONE && fm->dlg != DLG_MSG &&
        fm->dlg != DLG_PROPS && fm->dlg != DLG_DELETE) {
        if (ch >= 32 && ch < 127 && fm->dlg_cur < 62) {
            int len = (int)strlen(fm->dlg_buf);
            memmove(fm->dlg_buf + fm->dlg_cur + 1,
                    fm->dlg_buf + fm->dlg_cur,
                    (unsigned)(len - fm->dlg_cur + 1));
            fm->dlg_buf[fm->dlg_cur++] = ch;
        }
        return;
    }

    /* Type-ahead filter */
    if (ch >= 32 && ch < 127 && fm->filter_len < 30 && !fm->dlg) {
        fm->filter[fm->filter_len++] = ch;
        fm->filter[fm->filter_len] = '\0';
        rebuild_view(fm);
        if (fm->view_count > 0) select_single(fm, 0);
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

    int vis;
    if (fm->view_mode == VIEW_LIST) {
        vis = (fm->ch - TOOLBAR_H - COLHDR_H - STATUSBAR_H) / ROW_H;
    } else {
        int lw = list_area_w(fm);
        int cols = lw / GRID_CELL_W;
        if (cols < 1) cols = 1;
        int rows = (fm->view_count + cols - 1) / cols;
        int list_h = fm->ch - TOOLBAR_H - STATUSBAR_H;
        int vis_rows = list_h / GRID_CELL_H;
        vis = vis_rows * cols;
        int max_scroll = rows * cols - vis;
        if (max_scroll < 0) max_scroll = 0;
        if (fm->scroll > max_scroll) fm->scroll = max_scroll;
        return;
    }
    int max_scroll = fm->view_count - vis;
    if (max_scroll < 0) max_scroll = 0;
    if (fm->scroll > max_scroll) fm->scroll = max_scroll;
}

/*──────────────────────────────────────────────────────────────────
 * Lifecycle
 *────────────────────────────────────────────────────────────────*/
static void fm_destroy(AppContent *self)
{
    if (self->data) {
        FMData *fm = (FMData *)self->data;
        hal_image_free(&fm->preview_thumb);
        kfree(self->data);
    }
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
    fm->sel_anchor = -1;
    fm->preview_for_idx = -1;
    fm->cw = 498;
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
