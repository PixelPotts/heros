#include "terminal.h"
#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"
#include "../../kernel/sched.h"
#include "../../kernel/timer.h"
#include "../../kernel/kprintf.h"
#include "../../hal/hal_fs.h"

#define TERM_COLS    80
#define TERM_ROWS    25
#define MAX_INPUT    256
#define MAX_HISTORY  16
#define CHAR_W       6
#define CHAR_H       10
#define ED_ROWS      22       /* visible editor lines (rows 1-22) */
#define ED_BUF_INIT  8192     /* initial editor buffer size */

typedef struct {
    char     screen[TERM_ROWS][TERM_COLS + 1];
    int      cursor_row;
    int      cursor_col;
    int      scroll_offset;
    char     input[MAX_INPUT];
    int      input_len;
    int      input_cursor;
    char     cwd[256];
    /* History */
    char     history[MAX_HISTORY][MAX_INPUT];
    int      history_count;
    int      history_idx;
    /* Editor (nano) state */
    int      ed_active;
    char    *ed_buf;
    int      ed_cap;
    int      ed_len;
    int      ed_cx, ed_cy;
    int      ed_scroll;
    char     ed_file[256];
    int      ed_dirty;
    int      ed_nlines;
    char     ed_status[80];
    uint32_t ed_status_time;
} TermData;

static void term_clear_screen(TermData *td)
{
    for (int r = 0; r < TERM_ROWS; r++) {
        memset(td->screen[r], ' ', TERM_COLS);
        td->screen[r][TERM_COLS] = '\0';
    }
    td->cursor_row = 0;
    td->cursor_col = 0;
}

static void term_scroll_up(TermData *td)
{
    for (int r = 0; r < TERM_ROWS - 1; r++)
        memcpy(td->screen[r], td->screen[r + 1], TERM_COLS + 1);
    memset(td->screen[TERM_ROWS - 1], ' ', TERM_COLS);
    td->screen[TERM_ROWS - 1][TERM_COLS] = '\0';
}

static void term_putchar(TermData *td, char c)
{
    if (c == '\n') {
        td->cursor_col = 0;
        td->cursor_row++;
        if (td->cursor_row >= TERM_ROWS) {
            term_scroll_up(td);
            td->cursor_row = TERM_ROWS - 1;
        }
    } else if (c == '\r') {
        td->cursor_col = 0;
    } else if (c == '\b') {
        if (td->cursor_col > 0) td->cursor_col--;
    } else {
        if (td->cursor_col >= TERM_COLS) {
            td->cursor_col = 0;
            td->cursor_row++;
            if (td->cursor_row >= TERM_ROWS) {
                term_scroll_up(td);
                td->cursor_row = TERM_ROWS - 1;
            }
        }
        td->screen[td->cursor_row][td->cursor_col++] = c;
    }
}

static void term_puts(TermData *td, const char *s)
{
    while (*s) term_putchar(td, *s++);
}

static void term_prompt(TermData *td)
{
    const ThemeColors *tc = theme_colors();
    (void)tc;
    term_puts(td, td->cwd);
    term_puts(td, " $ ");
}

/* Forward declarations */
static void resolve_arg(TermData *td, const char *arg, char *out, int out_size);

/* ── Built-in commands ───────────────────────────────────────── */

static void cmd_help(TermData *td)
{
    term_puts(td, "Available commands:\n");
    term_puts(td, " File system:\n");
    term_puts(td, "  ls             List files\n");
    term_puts(td, "  cd <dir>       Change directory\n");
    term_puts(td, "  pwd            Print working directory\n");
    term_puts(td, "  cat <file>     Show file contents\n");
    term_puts(td, "  head <file>    Show first 10 lines\n");
    term_puts(td, "  wc <file>      Word/line/byte count\n");
    term_puts(td, "  stat <path>    File info\n");
    term_puts(td, "  touch <file>   Create empty file\n");
    term_puts(td, "  nano <file>    Text editor\n");
    term_puts(td, "  write <f> <t>  Write text to file\n");
    term_puts(td, "  mkdir <dir>    Create directory\n");
    term_puts(td, "  rm <file>      Delete file\n");
    term_puts(td, "  rmdir <dir>    Remove directory\n");
    term_puts(td, "  mv <src> <dst> Move/rename\n");
    term_puts(td, "  cp <src> <dst> Copy file\n");
    term_puts(td, " System:\n");
    term_puts(td, "  echo <msg>     Print message\n");
    term_puts(td, "  clear          Clear screen\n");
    term_puts(td, "  uname          System info\n");
    term_puts(td, "  uptime         System uptime\n");
    term_puts(td, "  free           Memory info\n");
    term_puts(td, "  ps             Process list\n");
    term_puts(td, "  theme [id]     List/set themes\n");
    term_puts(td, "  apps           List applications\n");
    term_puts(td, "  help           Show this help\n");
}

static void cmd_echo(TermData *td, const char *args)
{
    if (args && *args)
        term_puts(td, args);
    term_putchar(td, '\n');
}

static void cmd_ls(TermData *td, const char *args)
{
    char path[256];
    if (args && *args)
        resolve_arg(td, args, path, 256);
    else
        strncpy(path, td->cwd, 255);
    path[255] = '\0';

    int fd = hal_fs_open(path, FS_O_READ);
    if (fd < 0) {
        term_puts(td, "ls: cannot open '");
        term_puts(td, path);
        term_puts(td, "'\n");
        return;
    }

    fs_dirent_t entry;
    while (hal_fs_readdir(fd, &entry) > 0) {
        if (entry.type == FS_TYPE_DIR) {
            term_puts(td, "  [DIR]  ");
        } else {
            /* Right-align size in 7 chars */
            char sz[16];
            utoa(entry.size, sz, 10);
            int pad = 7 - (int)strlen(sz);
            for (int i = 0; i < pad; i++) term_putchar(td, ' ');
            term_puts(td, sz);
            term_puts(td, "  ");
        }
        term_puts(td, entry.name);
        term_putchar(td, '\n');
    }
    hal_fs_close(fd);
}

static void cmd_cat(TermData *td, const char *filename)
{
    if (!filename || !*filename) {
        term_puts(td, "cat: missing filename\n");
        return;
    }

    /* Build path */
    char path[256];
    if (filename[0] == '/') {
        strncpy(path, filename, sizeof(path) - 1);
    } else {
        strncpy(path, td->cwd, sizeof(path) - 1);
        if (path[strlen(path) - 1] != '/')
            strcat(path, "/");
        strcat(path, filename);
    }

    int fd = hal_fs_open(path, FS_O_READ);
    if (fd < 0) {
        term_puts(td, "cat: cannot open '");
        term_puts(td, filename);
        term_puts(td, "'\n");
        return;
    }

    char buf[128];
    int n;
    while ((n = hal_fs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        term_puts(td, buf);
    }
    term_putchar(td, '\n');
    hal_fs_close(fd);
}

static void cmd_uname(TermData *td)
{
    term_puts(td, "HerOS v1.0 RISC-V RV32IM bare-metal\n");
}

static void cmd_uptime(TermData *td)
{
    uint32_t ms = hal_get_ticks();
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    char buf[32];

    term_puts(td, "up ");
    itoa((int)hours, buf, 10);
    term_puts(td, buf);
    term_puts(td, "h ");
    itoa((int)(mins % 60), buf, 10);
    term_puts(td, buf);
    term_puts(td, "m ");
    itoa((int)(secs % 60), buf, 10);
    term_puts(td, buf);
    term_puts(td, "s\n");
}

static void cmd_free(TermData *td)
{
    size_t total = mm_total_pages();
    size_t free_p = mm_free_pages();
    size_t used = total - free_p;
    char buf[32];

    term_puts(td, "Total: ");
    utoa((unsigned)(total * 4), buf, 10);
    term_puts(td, buf);
    term_puts(td, " KB\nUsed:  ");
    utoa((unsigned)(used * 4), buf, 10);
    term_puts(td, buf);
    term_puts(td, " KB\nFree:  ");
    utoa((unsigned)(free_p * 4), buf, 10);
    term_puts(td, buf);
    term_puts(td, " KB\n");
}

static void cmd_ps(TermData *td)
{
    term_puts(td, "PID  STATE    NAME\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        task_state_t state = sched_task_state(i);
        if (state == TASK_FREE) continue;
        char buf[8];
        itoa(i, buf, 10);
        term_puts(td, " ");
        term_puts(td, buf);
        term_puts(td, "   ");
        switch (state) {
        case TASK_READY:    term_puts(td, "READY    "); break;
        case TASK_RUNNING:  term_puts(td, "RUNNING  "); break;
        case TASK_BLOCKED:  term_puts(td, "BLOCKED  "); break;
        case TASK_SLEEPING: term_puts(td, "SLEEPING "); break;
        case TASK_DEAD:     term_puts(td, "DEAD     "); break;
        default:            term_puts(td, "???      "); break;
        }
        term_puts(td, sched_task_name(i));
        term_putchar(td, '\n');
    }
}

static void cmd_theme(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "Themes:\n");
        for (int i = 0; i < THEME_COUNT; i++) {
            term_puts(td, "  ");
            char buf[4];
            itoa(i, buf, 10);
            term_puts(td, buf);
            term_puts(td, ": ");
            term_puts(td, theme_name(i));
            if (i == (int)theme_current())
                term_puts(td, " (active)");
            term_putchar(td, '\n');
        }
        term_puts(td, "Usage: theme <id>\n");
        return;
    }
    int id = atoi(args);
    if (id >= 0 && id < THEME_COUNT) {
        theme_set((ThemeId)id);
        term_puts(td, "Theme set to: ");
        term_puts(td, theme_name(id));
        term_putchar(td, '\n');
    } else {
        term_puts(td, "Invalid theme ID\n");
    }
}

static void cmd_apps(TermData *td)
{
    term_puts(td, "Installed apps:\n");
    term_puts(td, "  Terminal\n");
    term_puts(td, "  File Manager\n");
    term_puts(td, "  Settings\n");
    term_puts(td, "  Task Manager\n");
}

/* Build full path from cwd + relative/absolute arg */
static void resolve_arg(TermData *td, const char *arg, char *out, int out_size)
{
    if (!arg || !*arg) {
        strncpy(out, td->cwd, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    if (arg[0] == '/') {
        strncpy(out, arg, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        strncpy(out, td->cwd, out_size - 1);
        out[out_size - 1] = '\0';
        if (out[strlen(out) - 1] != '/')
            strcat(out, "/");
        strcat(out, arg);
    }
}

static void cmd_cd(TermData *td, const char *args)
{
    if (!args || !*args) {
        strcpy(td->cwd, "/");
        return;
    }

    char path[256];
    if (strcmp(args, "..") == 0) {
        strncpy(path, td->cwd, 255);
        char *sl = strrchr(path, '/');
        if (sl && sl != path) *sl = '\0';
        else strcpy(path, "/");
    } else if (strcmp(args, "/") == 0) {
        strcpy(path, "/");
    } else {
        resolve_arg(td, args, path, 256);
    }

    /* Verify directory exists */
    fs_stat_t st;
    if (hal_fs_stat(path, &st) == 0 && st.type == FS_TYPE_DIR) {
        strncpy(td->cwd, path, 255);
        td->cwd[255] = '\0';
    } else {
        /* Try as simple name in cwd */
        if (args[0] != '/' && args[0] != '.') {
            char try[256];
            resolve_arg(td, args, try, 256);
            int fd = hal_fs_open(try, FS_O_READ);
            if (fd >= 0) {
                hal_fs_close(fd);
                /* Check if it's a directory by trying readdir */
                fd = hal_fs_open(try, FS_O_READ);
                if (fd >= 0) {
                    fs_dirent_t e;
                    int is_dir = (hal_fs_readdir(fd, &e) >= 0);
                    hal_fs_close(fd);
                    if (is_dir) {
                        strncpy(td->cwd, try, 255);
                        td->cwd[255] = '\0';
                        return;
                    }
                }
            }
        }
        term_puts(td, "cd: no such directory: ");
        term_puts(td, args);
        term_putchar(td, '\n');
    }
}

static void cmd_mkdir(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "mkdir: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);
    if (hal_fs_mkdir(path) == 0) {
        term_puts(td, "Created: ");
        term_puts(td, args);
        term_putchar(td, '\n');
    } else {
        term_puts(td, "mkdir: cannot create '");
        term_puts(td, args);
        term_puts(td, "'\n");
    }
}

static void cmd_rm(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "rm: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);
    if (hal_fs_unlink(path) == 0) {
        term_puts(td, "Removed: ");
        term_puts(td, args);
        term_putchar(td, '\n');
    } else {
        term_puts(td, "rm: cannot remove '");
        term_puts(td, args);
        term_puts(td, "'\n");
    }
}

static void cmd_rmdir(TermData *td, const char *args)
{
    /* rmdir is same as rm for our FAT16 (unlink handles dirs) */
    if (!args || !*args) {
        term_puts(td, "rmdir: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);
    if (hal_fs_unlink(path) == 0) {
        term_puts(td, "Removed directory: ");
        term_puts(td, args);
        term_putchar(td, '\n');
    } else {
        term_puts(td, "rmdir: cannot remove '");
        term_puts(td, args);
        term_puts(td, "'\n");
    }
}

static void cmd_touch(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "touch: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);
    int fd = hal_fs_open(path, FS_O_CREATE | FS_O_WRITE);
    if (fd >= 0) {
        hal_fs_close(fd);
    } else {
        term_puts(td, "touch: cannot create '");
        term_puts(td, args);
        term_puts(td, "'\n");
    }
}

static void cmd_mv(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "mv: missing operand\n");
        term_puts(td, "Usage: mv <source> <dest>\n");
        return;
    }

    /* Parse two arguments */
    const char *src_arg = args;
    const char *dst_arg = args;
    while (*dst_arg && *dst_arg != ' ') dst_arg++;
    if (!*dst_arg) {
        term_puts(td, "mv: missing destination\n");
        return;
    }

    /* Null-terminate src in a temp buffer */
    char src_name[64];
    int slen = (int)(dst_arg - src_arg);
    if (slen > 63) slen = 63;
    memcpy(src_name, src_arg, slen);
    src_name[slen] = '\0';

    while (*dst_arg == ' ') dst_arg++;
    if (!*dst_arg) {
        term_puts(td, "mv: missing destination\n");
        return;
    }

    char src_path[256], dst_path[256];
    resolve_arg(td, src_name, src_path, 256);
    resolve_arg(td, dst_arg, dst_path, 256);

    if (hal_fs_rename(src_path, dst_path) == 0) {
        term_puts(td, "Moved: ");
        term_puts(td, src_name);
        term_puts(td, " -> ");
        term_puts(td, dst_arg);
        term_putchar(td, '\n');
    } else {
        term_puts(td, "mv: cannot move '");
        term_puts(td, src_name);
        term_puts(td, "'\n");
    }
}

static void cmd_cp(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "cp: missing operand\n");
        term_puts(td, "Usage: cp <source> <dest>\n");
        return;
    }

    /* Parse two arguments */
    const char *src_arg = args;
    const char *dst_arg = args;
    while (*dst_arg && *dst_arg != ' ') dst_arg++;
    if (!*dst_arg) {
        term_puts(td, "cp: missing destination\n");
        return;
    }

    char src_name[64];
    int slen = (int)(dst_arg - src_arg);
    if (slen > 63) slen = 63;
    memcpy(src_name, src_arg, slen);
    src_name[slen] = '\0';

    while (*dst_arg == ' ') dst_arg++;
    if (!*dst_arg) {
        term_puts(td, "cp: missing destination\n");
        return;
    }

    char src_path[256], dst_path[256];
    resolve_arg(td, src_name, src_path, 256);
    resolve_arg(td, dst_arg, dst_path, 256);

    /* Open source for reading */
    int src_fd = hal_fs_open(src_path, FS_O_READ);
    if (src_fd < 0) {
        term_puts(td, "cp: cannot open '");
        term_puts(td, src_name);
        term_puts(td, "'\n");
        return;
    }

    /* Create destination */
    int dst_fd = hal_fs_open(dst_path, FS_O_CREATE | FS_O_WRITE);
    if (dst_fd < 0) {
        hal_fs_close(src_fd);
        term_puts(td, "cp: cannot create '");
        term_puts(td, dst_arg);
        term_puts(td, "'\n");
        return;
    }

    /* Copy data */
    char buf[512];
    int n;
    int total = 0;
    while ((n = hal_fs_read(src_fd, buf, sizeof(buf))) > 0) {
        int w = hal_fs_write(dst_fd, buf, n);
        if (w < 0) break;
        total += w;
    }

    hal_fs_close(src_fd);
    hal_fs_close(dst_fd);

    term_puts(td, "Copied ");
    char sz[16];
    utoa((unsigned)total, sz, 10);
    term_puts(td, sz);
    term_puts(td, " bytes: ");
    term_puts(td, src_name);
    term_puts(td, " -> ");
    term_puts(td, dst_arg);
    term_putchar(td, '\n');
}

static void cmd_stat(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "stat: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);

    fs_stat_t st;
    if (hal_fs_stat(path, &st) != 0) {
        term_puts(td, "stat: cannot stat '");
        term_puts(td, args);
        term_puts(td, "'\n");
        return;
    }

    term_puts(td, "  File: ");
    term_puts(td, st.name);
    term_putchar(td, '\n');
    term_puts(td, "  Type: ");
    term_puts(td, st.type == FS_TYPE_DIR ? "directory" : "regular file");
    term_putchar(td, '\n');
    term_puts(td, "  Size: ");
    char buf[16];
    utoa(st.size, buf, 10);
    term_puts(td, buf);
    term_puts(td, " bytes\n");
    term_puts(td, "  Path: ");
    term_puts(td, path);
    term_putchar(td, '\n');
    term_puts(td, "    FS: FAT16\n");
}

static void cmd_write(TermData *td, const char *args)
{
    /* write <filename> <content> — write text to a file */
    if (!args || !*args) {
        term_puts(td, "write: missing operand\n");
        term_puts(td, "Usage: write <file> <text>\n");
        return;
    }

    const char *fname = args;
    const char *content = args;
    while (*content && *content != ' ') content++;
    if (!*content) {
        term_puts(td, "write: missing content\n");
        return;
    }

    char filename[64];
    int flen = (int)(content - fname);
    if (flen > 63) flen = 63;
    memcpy(filename, fname, flen);
    filename[flen] = '\0';

    while (*content == ' ') content++;

    char path[256];
    resolve_arg(td, filename, path, 256);

    int fd = hal_fs_open(path, FS_O_CREATE | FS_O_WRITE);
    if (fd < 0) {
        term_puts(td, "write: cannot open '");
        term_puts(td, filename);
        term_puts(td, "'\n");
        return;
    }

    int len = (int)strlen(content);
    int w = hal_fs_write(fd, content, len);
    /* Write a newline too */
    hal_fs_write(fd, "\n", 1);
    hal_fs_close(fd);

    if (w >= 0) {
        char buf[16];
        itoa(w + 1, buf, 10);
        term_puts(td, "Wrote ");
        term_puts(td, buf);
        term_puts(td, " bytes to ");
        term_puts(td, filename);
        term_putchar(td, '\n');
    } else {
        term_puts(td, "write: error writing\n");
    }
}

static void cmd_wc(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "wc: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);

    int fd = hal_fs_open(path, FS_O_READ);
    if (fd < 0) {
        term_puts(td, "wc: cannot open '");
        term_puts(td, args);
        term_puts(td, "'\n");
        return;
    }

    int bytes = 0, lines = 0, words = 0;
    int in_word = 0;
    char buf[256];
    int n;
    while ((n = hal_fs_read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            bytes++;
            if (buf[i] == '\n') lines++;
            if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }
    hal_fs_close(fd);

    char num[16];
    term_puts(td, "  ");
    itoa(lines, num, 10);
    term_puts(td, num);
    term_puts(td, " lines, ");
    itoa(words, num, 10);
    term_puts(td, num);
    term_puts(td, " words, ");
    itoa(bytes, num, 10);
    term_puts(td, num);
    term_puts(td, " bytes  ");
    term_puts(td, args);
    term_putchar(td, '\n');
}

static void cmd_head(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "head: missing operand\n");
        return;
    }
    char path[256];
    resolve_arg(td, args, path, 256);

    int fd = hal_fs_open(path, FS_O_READ);
    if (fd < 0) {
        term_puts(td, "head: cannot open '");
        term_puts(td, args);
        term_puts(td, "'\n");
        return;
    }

    /* Show first 10 lines */
    char buf[128];
    int n;
    int lines = 0;
    while (lines < 10 && (n = hal_fs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        for (int i = 0; i < n && lines < 10; i++) {
            term_putchar(td, buf[i]);
            if (buf[i] == '\n') lines++;
        }
    }
    if (lines == 0) term_putchar(td, '\n');
    hal_fs_close(fd);
}

/* ── Nano editor helpers ─────────────────────────────────────── */

static int ed_count_lines(const char *buf, int len)
{
    int n = 1;
    for (int i = 0; i < len; i++)
        if (buf[i] == '\n') n++;
    return n;
}

/* Return byte offset of start of line N (0-based) */
static int ed_line_start(const char *buf, int len, int line)
{
    if (line == 0) return 0;
    int cur = 0;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            cur++;
            if (cur == line) return i + 1;
        }
    }
    return len;
}

/* Length of line at given byte offset (not counting \n) */
static int ed_line_len(const char *buf, int len, int off)
{
    int l = 0;
    while (off + l < len && buf[off + l] != '\n')
        l++;
    return l;
}

/* Ensure buffer has room for `need` more bytes */
static void ed_grow(TermData *td, int need)
{
    while (td->ed_len + need >= td->ed_cap) {
        int newcap = td->ed_cap * 2;
        char *nb = (char *)kmalloc(newcap);
        if (!nb) return;
        memcpy(nb, td->ed_buf, td->ed_len);
        kfree(td->ed_buf);
        td->ed_buf = nb;
        td->ed_cap = newcap;
    }
}

static void ed_set_status(TermData *td, const char *msg)
{
    strncpy(td->ed_status, msg, 79);
    td->ed_status[79] = '\0';
    td->ed_status_time = hal_get_ticks();
}

/* Get cursor byte offset in buffer */
static int ed_cursor_off(TermData *td)
{
    int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
    int ll = ed_line_len(td->ed_buf, td->ed_len, off);
    int cx = td->ed_cx;
    if (cx > ll) cx = ll;
    return off + cx;
}

static void ed_insert_char(TermData *td, char ch)
{
    ed_grow(td, 1);
    int pos = ed_cursor_off(td);
    memmove(td->ed_buf + pos + 1, td->ed_buf + pos, td->ed_len - pos);
    td->ed_buf[pos] = ch;
    td->ed_len++;
    if (ch == '\n') {
        td->ed_cy++;
        td->ed_cx = 0;
        td->ed_nlines++;
    } else {
        td->ed_cx++;
    }
    td->ed_dirty = 1;
}

static void ed_delete_char(TermData *td)
{
    int pos = ed_cursor_off(td);
    if (pos >= td->ed_len) return;
    if (td->ed_buf[pos] == '\n')
        td->ed_nlines--;
    memmove(td->ed_buf + pos, td->ed_buf + pos + 1, td->ed_len - pos - 1);
    td->ed_len--;
    td->ed_dirty = 1;
}

static void ed_backspace(TermData *td)
{
    if (td->ed_cx == 0 && td->ed_cy == 0) return;
    if (td->ed_cx > 0) {
        td->ed_cx--;
        ed_delete_char(td);
    } else {
        /* Merge with previous line */
        td->ed_cy--;
        int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
        int ll = ed_line_len(td->ed_buf, td->ed_len, off);
        td->ed_cx = ll;
        ed_delete_char(td);  /* delete the \n at end of prev line */
    }
}

static void ed_cut_line(TermData *td)
{
    int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
    int ll = ed_line_len(td->ed_buf, td->ed_len, off);
    int del = ll;
    /* Also delete the trailing \n if present */
    if (off + ll < td->ed_len && td->ed_buf[off + ll] == '\n')
        del++;
    if (del == 0) return;
    memmove(td->ed_buf + off, td->ed_buf + off + del, td->ed_len - off - del);
    td->ed_len -= del;
    td->ed_nlines = ed_count_lines(td->ed_buf, td->ed_len);
    /* Adjust cursor */
    if (td->ed_cy >= td->ed_nlines && td->ed_cy > 0)
        td->ed_cy--;
    td->ed_cx = 0;
    td->ed_dirty = 1;
    ed_set_status(td, "Line cut");
}

static void ed_save(TermData *td)
{
    int fd = hal_fs_open(td->ed_file, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (fd < 0) {
        ed_set_status(td, "Error: cannot save file");
        return;
    }
    if (td->ed_len > 0)
        hal_fs_write(fd, td->ed_buf, td->ed_len);
    hal_fs_close(fd);
    td->ed_dirty = 0;
    char msg[80];
    strcpy(msg, "Saved ");
    char num[16];
    itoa(td->ed_len, num, 10);
    strcat(msg, num);
    strcat(msg, " bytes");
    ed_set_status(td, msg);
}

static void ed_close(TermData *td)
{
    if (td->ed_buf) {
        kfree(td->ed_buf);
        td->ed_buf = (void *)0;
    }
    td->ed_active = 0;
    td->ed_len = 0;
    td->ed_cap = 0;
}

static void ed_clamp_cursor(TermData *td)
{
    if (td->ed_cy < 0) td->ed_cy = 0;
    if (td->ed_cy >= td->ed_nlines) td->ed_cy = td->ed_nlines - 1;
    if (td->ed_cy < 0) td->ed_cy = 0;
    int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
    int ll = ed_line_len(td->ed_buf, td->ed_len, off);
    if (td->ed_cx > ll) td->ed_cx = ll;
    if (td->ed_cx < 0) td->ed_cx = 0;
    /* Scroll to keep cursor visible */
    if (td->ed_cy < td->ed_scroll)
        td->ed_scroll = td->ed_cy;
    if (td->ed_cy >= td->ed_scroll + ED_ROWS)
        td->ed_scroll = td->ed_cy - ED_ROWS + 1;
}

static void ed_render(TermData *td, Rect cr)
{
    const ThemeColors *tc = theme_colors();
    /* Dark background */
    draw_filled_rect(cr, COLOR(10, 10, 20, 255));

    /* Row 0: title bar */
    Rect title_r = RECT(cr.x, cr.y + 4, cr.w, CHAR_H);
    draw_filled_rect(title_r, COLOR(40, 40, 80, 255));
    char title[80];
    strcpy(title, " nano: ");
    /* Show just filename, not full path */
    const char *fname = td->ed_file;
    const char *sl = strrchr(td->ed_file, '/');
    if (sl && sl[1]) fname = sl + 1;
    strcat(title, fname);
    if (td->ed_dirty) strcat(title, " [Modified]");
    draw_text(cr.x + 4, cr.y + 4, title, COLOR(255, 255, 255, 255), FONT_SIZE_SMALL);

    /* Rows 1-22: file content */
    for (int r = 0; r < ED_ROWS; r++) {
        int line = td->ed_scroll + r;
        int y = cr.y + 4 + (r + 1) * CHAR_H;
        if (y + CHAR_H > cr.y + cr.h) break;

        if (line < td->ed_nlines) {
            int off = ed_line_start(td->ed_buf, td->ed_len, line);
            int ll = ed_line_len(td->ed_buf, td->ed_len, off);
            int max_c = TERM_COLS;
            if (ll < max_c) max_c = ll;
            for (int c = 0; c < max_c; c++) {
                char ch = td->ed_buf[off + c];
                if (ch >= 32 && ch <= 126) {
                    char cc[2] = {ch, '\0'};
                    int x = cr.x + 4 + c * CHAR_W;
                    draw_text(x, y, cc, tc->text_primary, FONT_SIZE_SMALL);
                }
            }
        } else {
            /* Draw ~ for lines past end of file */
            draw_text(cr.x + 4, y, "~", COLOR(100, 100, 140, 255), FONT_SIZE_SMALL);
        }
    }

    /* Cursor */
    int cur_screen_row = td->ed_cy - td->ed_scroll;
    if (cur_screen_row >= 0 && cur_screen_row < ED_ROWS) {
        if ((hal_get_ticks() / 500) % 2 == 0) {
            int cx_px = cr.x + 4 + td->ed_cx * CHAR_W;
            int cy_px = cr.y + 4 + (cur_screen_row + 1) * CHAR_H;
            draw_filled_rect(RECT(cx_px, cy_px, 2, CHAR_H), tc->accent);
        }
    }

    /* Row 23: status line */
    int status_y = cr.y + 4 + 23 * CHAR_H;
    Rect status_r = RECT(cr.x, status_y, cr.w, CHAR_H);
    draw_filled_rect(status_r, COLOR(40, 40, 80, 255));

    /* Show transient status message or position info */
    char sline[80];
    if (td->ed_status[0] && (hal_get_ticks() - td->ed_status_time < 3000)) {
        strncpy(sline, td->ed_status, 79);
    } else {
        td->ed_status[0] = '\0';
        strcpy(sline, " Line ");
        char num[16];
        itoa(td->ed_cy + 1, num, 10);
        strcat(sline, num);
        strcat(sline, ", Col ");
        itoa(td->ed_cx + 1, num, 10);
        strcat(sline, num);
        strcat(sline, " | ");
        itoa(td->ed_len, num, 10);
        strcat(sline, num);
        strcat(sline, " bytes");
    }
    sline[79] = '\0';
    draw_text(cr.x + 4, status_y, sline, COLOR(200, 200, 255, 255), FONT_SIZE_SMALL);

    /* Row 24: help bar */
    int help_y = cr.y + 4 + 24 * CHAR_H;
    Rect help_r = RECT(cr.x, help_y, cr.w, CHAR_H);
    draw_filled_rect(help_r, COLOR(30, 30, 60, 255));
    draw_text(cr.x + 4, help_y,
              "^S Save  ^X Exit  ^K Cut Line  ^G Goto  Enter New Line",
              COLOR(180, 180, 220, 255), FONT_SIZE_SMALL);
}

static void ed_on_key(TermData *td, uint16_t key, uint16_t mod)
{
    int ctrl = (mod & HAL_MOD_CTRL) != 0;

    if (ctrl) {
        switch (key) {
        case HAL_KEY_S:
            ed_save(td);
            return;
        case HAL_KEY_X:
            ed_close(td);
            term_puts(td, "\n");
            term_prompt(td);
            return;
        case HAL_KEY_K:
            ed_cut_line(td);
            ed_clamp_cursor(td);
            return;
        case HAL_KEY_G: {
            /* Goto — jump to top for now (simple impl) */
            td->ed_cy = 0;
            td->ed_cx = 0;
            td->ed_scroll = 0;
            ed_set_status(td, "Jumped to top");
            return;
        }
        default:
            break;
        }
    }

    switch (key) {
    case HAL_KEY_UP:
        td->ed_cy--;
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_DOWN:
        td->ed_cy++;
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_LEFT:
        if (td->ed_cx > 0) {
            td->ed_cx--;
        } else if (td->ed_cy > 0) {
            td->ed_cy--;
            int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
            td->ed_cx = ed_line_len(td->ed_buf, td->ed_len, off);
        }
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_RIGHT: {
        int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
        int ll = ed_line_len(td->ed_buf, td->ed_len, off);
        if (td->ed_cx < ll) {
            td->ed_cx++;
        } else if (td->ed_cy < td->ed_nlines - 1) {
            td->ed_cy++;
            td->ed_cx = 0;
        }
        ed_clamp_cursor(td);
        break;
    }
    case HAL_KEY_HOME:
        td->ed_cx = 0;
        break;
    case HAL_KEY_END: {
        int off = ed_line_start(td->ed_buf, td->ed_len, td->ed_cy);
        td->ed_cx = ed_line_len(td->ed_buf, td->ed_len, off);
        break;
    }
    case HAL_KEY_PAGEUP:
        td->ed_cy -= ED_ROWS;
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_PAGEDOWN:
        td->ed_cy += ED_ROWS;
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_BACKSPACE:
        ed_backspace(td);
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_DELETE:
        ed_delete_char(td);
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_RETURN:
        ed_insert_char(td, '\n');
        ed_clamp_cursor(td);
        break;
    case HAL_KEY_TAB:
        /* Insert 4 spaces */
        for (int i = 0; i < 4; i++)
            ed_insert_char(td, ' ');
        ed_clamp_cursor(td);
        break;
    default:
        break;
    }
}

static void ed_on_text(TermData *td, char ch)
{
    if (ch >= 32 && ch < 127) {
        ed_insert_char(td, ch);
        ed_clamp_cursor(td);
    }
}

static void cmd_nano(TermData *td, const char *args)
{
    if (!args || !*args) {
        term_puts(td, "nano: missing filename\n");
        term_puts(td, "Usage: nano <file>\n");
        return;
    }

    /* Resolve path */
    resolve_arg(td, args, td->ed_file, 256);

    /* Allocate buffer */
    td->ed_buf = (char *)kmalloc(ED_BUF_INIT);
    if (!td->ed_buf) {
        term_puts(td, "nano: out of memory\n");
        return;
    }
    td->ed_cap = ED_BUF_INIT;
    td->ed_len = 0;

    /* Try to load existing file */
    int fd = hal_fs_open(td->ed_file, FS_O_READ);
    if (fd >= 0) {
        char rbuf[512];
        int n;
        while ((n = hal_fs_read(fd, rbuf, sizeof(rbuf))) > 0) {
            ed_grow(td, n);
            memcpy(td->ed_buf + td->ed_len, rbuf, n);
            td->ed_len += n;
        }
        hal_fs_close(fd);
    }
    /* else: new file, empty buffer */

    td->ed_cx = 0;
    td->ed_cy = 0;
    td->ed_scroll = 0;
    td->ed_dirty = 0;
    td->ed_nlines = ed_count_lines(td->ed_buf, td->ed_len);
    td->ed_status[0] = '\0';
    td->ed_status_time = 0;
    td->ed_active = 1;

    if (fd < 0)
        ed_set_status(td, "New file");
    else {
        char msg[80];
        strcpy(msg, "Loaded ");
        char num[16];
        itoa(td->ed_len, num, 10);
        strcat(msg, num);
        strcat(msg, " bytes");
        ed_set_status(td, msg);
    }
}

static void execute_command(TermData *td, const char *cmd)
{
    /* Save to history */
    if (td->history_count < MAX_HISTORY) {
        strncpy(td->history[td->history_count], cmd, MAX_INPUT - 1);
        td->history_count++;
    }
    td->history_idx = td->history_count;

    /* Skip whitespace */
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    /* Find args */
    const char *args = cmd;
    while (*args && *args != ' ') args++;
    int cmd_len = (int)(args - cmd);
    while (*args == ' ') args++;

    if (strncmp(cmd, "help", cmd_len) == 0 && cmd_len == 4)
        cmd_help(td);
    else if (strncmp(cmd, "clear", cmd_len) == 0 && cmd_len == 5)
        term_clear_screen(td);
    else if (strncmp(cmd, "echo", cmd_len) == 0 && cmd_len == 4)
        cmd_echo(td, args);
    else if (strncmp(cmd, "ls", cmd_len) == 0 && cmd_len == 2)
        cmd_ls(td, args);
    else if (strncmp(cmd, "cat", cmd_len) == 0 && cmd_len == 3)
        cmd_cat(td, args);
    else if (strncmp(cmd, "cd", cmd_len) == 0 && cmd_len == 2)
        cmd_cd(td, args);
    else if (strncmp(cmd, "pwd", cmd_len) == 0 && cmd_len == 3) {
        term_puts(td, td->cwd);
        term_putchar(td, '\n');
    }
    else if (strncmp(cmd, "mkdir", cmd_len) == 0 && cmd_len == 5)
        cmd_mkdir(td, args);
    else if (strncmp(cmd, "rmdir", cmd_len) == 0 && cmd_len == 5)
        cmd_rmdir(td, args);
    else if (strncmp(cmd, "rm", cmd_len) == 0 && cmd_len == 2)
        cmd_rm(td, args);
    else if (strncmp(cmd, "touch", cmd_len) == 0 && cmd_len == 5)
        cmd_touch(td, args);
    else if (strncmp(cmd, "mv", cmd_len) == 0 && cmd_len == 2)
        cmd_mv(td, args);
    else if (strncmp(cmd, "cp", cmd_len) == 0 && cmd_len == 2)
        cmd_cp(td, args);
    else if (strncmp(cmd, "stat", cmd_len) == 0 && cmd_len == 4)
        cmd_stat(td, args);
    else if (strncmp(cmd, "write", cmd_len) == 0 && cmd_len == 5)
        cmd_write(td, args);
    else if (strncmp(cmd, "wc", cmd_len) == 0 && cmd_len == 2)
        cmd_wc(td, args);
    else if (strncmp(cmd, "head", cmd_len) == 0 && cmd_len == 4)
        cmd_head(td, args);
    else if (strncmp(cmd, "uname", cmd_len) == 0 && cmd_len == 5)
        cmd_uname(td);
    else if (strncmp(cmd, "uptime", cmd_len) == 0 && cmd_len == 6)
        cmd_uptime(td);
    else if (strncmp(cmd, "free", cmd_len) == 0 && cmd_len == 4)
        cmd_free(td);
    else if (strncmp(cmd, "ps", cmd_len) == 0 && cmd_len == 2)
        cmd_ps(td);
    else if (strncmp(cmd, "theme", cmd_len) == 0 && cmd_len == 5)
        cmd_theme(td, args);
    else if (strncmp(cmd, "apps", cmd_len) == 0 && cmd_len == 4)
        cmd_apps(td);
    else if (strncmp(cmd, "nano", cmd_len) == 0 && cmd_len == 4)
        cmd_nano(td, args);
    else {
        term_puts(td, "Unknown command: ");
        for (int i = 0; i < cmd_len; i++)
            term_putchar(td, cmd[i]);
        term_puts(td, "\nType 'help' for commands.\n");
    }
}

/* ── AppContent callbacks ────────────────────────────────────── */

static void term_render(AppContent *self, Rect cr)
{
    TermData *td = (TermData *)self->data;
    if (td->ed_active) { ed_render(td, cr); return; }
    const ThemeColors *tc = theme_colors();

    /* Terminal background */
    draw_filled_rect(cr, COLOR(10, 10, 20, 255));

    /* Render screen buffer */
    for (int r = 0; r < TERM_ROWS; r++) {
        int y = cr.y + 4 + r * CHAR_H;
        if (y + CHAR_H > cr.y + cr.h) break;

        for (int c = 0; c < TERM_COLS; c++) {
            char ch = td->screen[r][c];
            if (ch > ' ' && ch <= '~') {
                int x = cr.x + 4 + c * CHAR_W;
                if (x + CHAR_W > cr.x + cr.w) break;

                Color fc = tc->text_primary;
                /* Simple ANSI-like coloring: $ prompt in green */
                char cc[2] = {ch, '\0'};
                draw_text(x, y, cc, fc, FONT_SIZE_SMALL);
            }
        }
    }

    /* Input line */
    int input_y = cr.y + 4 + td->cursor_row * CHAR_H;
    int prompt_len = (int)strlen(td->cwd) + 3;  /* "/ $ " */

    /* Render current input at cursor position */
    int input_x = cr.x + 4 + td->cursor_col * CHAR_W;
    for (int i = 0; i < td->input_len; i++) {
        char cc[2] = {td->input[i], '\0'};
        draw_text(input_x + i * CHAR_W, input_y, cc,
                  tc->accent, FONT_SIZE_SMALL);
    }

    /* Blinking cursor */
    if ((hal_get_ticks() / 500) % 2 == 0) {
        int cx = input_x + td->input_cursor * CHAR_W;
        draw_filled_rect(RECT(cx, input_y, 2, CHAR_H), tc->accent);
    }

    (void)prompt_len;
}

static void term_on_text_input(AppContent *self, char ch)
{
    TermData *td = (TermData *)self->data;
    if (td->ed_active) { ed_on_text(td, ch); return; }
    if (ch >= 32 && ch < 127 && td->input_len < MAX_INPUT - 1) {
        /* Insert at cursor */
        for (int i = td->input_len; i > td->input_cursor; i--)
            td->input[i] = td->input[i - 1];
        td->input[td->input_cursor] = ch;
        td->input_len++;
        td->input_cursor++;
        td->input[td->input_len] = '\0';
    }
}

static void term_on_key_down(AppContent *self, uint16_t key, uint16_t mod)
{
    TermData *td = (TermData *)self->data;
    if (td->ed_active) { ed_on_key(td, key, mod); return; }
    (void)mod;

    switch (key) {
    case HAL_KEY_RETURN:
        /* Echo input to screen */
        for (int i = 0; i < td->input_len; i++)
            term_putchar(td, td->input[i]);
        term_putchar(td, '\n');

        /* Execute */
        td->input[td->input_len] = '\0';
        execute_command(td, td->input);

        /* Reset input */
        td->input_len = 0;
        td->input_cursor = 0;
        td->input[0] = '\0';

        /* New prompt */
        term_prompt(td);
        break;

    case HAL_KEY_BACKSPACE:
        if (td->input_cursor > 0) {
            for (int i = td->input_cursor - 1; i < td->input_len - 1; i++)
                td->input[i] = td->input[i + 1];
            td->input_len--;
            td->input_cursor--;
            td->input[td->input_len] = '\0';
        }
        break;

    case HAL_KEY_DELETE:
        if (td->input_cursor < td->input_len) {
            for (int i = td->input_cursor; i < td->input_len - 1; i++)
                td->input[i] = td->input[i + 1];
            td->input_len--;
            td->input[td->input_len] = '\0';
        }
        break;

    case HAL_KEY_LEFT:
        if (td->input_cursor > 0) td->input_cursor--;
        break;

    case HAL_KEY_RIGHT:
        if (td->input_cursor < td->input_len) td->input_cursor++;
        break;

    case HAL_KEY_HOME:
        td->input_cursor = 0;
        break;

    case HAL_KEY_END:
        td->input_cursor = td->input_len;
        break;

    case HAL_KEY_UP:
        if (td->history_idx > 0) {
            td->history_idx--;
            strncpy(td->input, td->history[td->history_idx], MAX_INPUT - 1);
            td->input_len = (int)strlen(td->input);
            td->input_cursor = td->input_len;
        }
        break;

    case HAL_KEY_DOWN:
        if (td->history_idx < td->history_count - 1) {
            td->history_idx++;
            strncpy(td->input, td->history[td->history_idx], MAX_INPUT - 1);
            td->input_len = (int)strlen(td->input);
            td->input_cursor = td->input_len;
        } else {
            td->history_idx = td->history_count;
            td->input_len = 0;
            td->input_cursor = 0;
            td->input[0] = '\0';
        }
        break;
    }
}

static void term_destroy(AppContent *self)
{
    TermData *td = (TermData *)self->data;
    if (td) {
        if (td->ed_buf) kfree(td->ed_buf);
        kfree(td);
    }
    kfree(self);
}

AppContent *terminal_create(void)
{
    AppContent *app = (AppContent *)kmalloc(sizeof(AppContent));
    if (!app) return (void *)0;
    memset(app, 0, sizeof(AppContent));

    TermData *td = (TermData *)kmalloc(sizeof(TermData));
    if (!td) { kfree(app); return (void *)0; }
    memset(td, 0, sizeof(TermData));

    term_clear_screen(td);
    strcpy(td->cwd, "/");

    /* Welcome message */
    term_puts(td, "HerOS Terminal v1.0\n");
    term_puts(td, "Type 'help' for commands.\n\n");
    term_prompt(td);

    app->render = term_render;
    app->on_text_input = term_on_text_input;
    app->on_key_down = term_on_key_down;
    app->destroy = term_destroy;
    app->data = td;

    return app;
}
