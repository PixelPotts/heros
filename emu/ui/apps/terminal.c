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

/* ── Built-in commands ───────────────────────────────────────── */

static void cmd_help(TermData *td)
{
    term_puts(td, "Available commands:\n");
    term_puts(td, "  help        Show this help\n");
    term_puts(td, "  clear       Clear screen\n");
    term_puts(td, "  echo <msg>  Print message\n");
    term_puts(td, "  ls          List files\n");
    term_puts(td, "  cat <file>  Show file contents\n");
    term_puts(td, "  pwd         Print working directory\n");
    term_puts(td, "  uname       System info\n");
    term_puts(td, "  uptime      System uptime\n");
    term_puts(td, "  free        Memory info\n");
    term_puts(td, "  ps          Process list\n");
    term_puts(td, "  theme       List/set themes\n");
    term_puts(td, "  apps        List applications\n");
}

static void cmd_echo(TermData *td, const char *args)
{
    if (args && *args)
        term_puts(td, args);
    term_putchar(td, '\n');
}

static void cmd_ls(TermData *td)
{
    int fd = hal_fs_open(td->cwd, FS_O_READ);
    if (fd < 0) {
        term_puts(td, "ls: cannot open directory\n");
        return;
    }

    fs_dirent_t entry;
    while (hal_fs_readdir(fd, &entry) > 0) {
        if (entry.type == FS_TYPE_DIR) {
            term_puts(td, "  [DIR]  ");
        } else {
            term_puts(td, "         ");
        }
        term_puts(td, entry.name);
        if (entry.type == FS_TYPE_FILE && entry.size > 0) {
            term_puts(td, "  (");
            char sz[16];
            utoa(entry.size, sz, 10);
            term_puts(td, sz);
            term_puts(td, " bytes)");
        }
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
    /* Will be populated from app_registry */
    term_puts(td, "  Terminal\n");
    term_puts(td, "  File Manager\n");
    term_puts(td, "  Settings\n");
    term_puts(td, "  Task Manager\n");
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
        cmd_ls(td);
    else if (strncmp(cmd, "cat", cmd_len) == 0 && cmd_len == 3)
        cmd_cat(td, args);
    else if (strncmp(cmd, "pwd", cmd_len) == 0 && cmd_len == 3) {
        term_puts(td, td->cwd);
        term_putchar(td, '\n');
    }
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
    else {
        term_puts(td, "Unknown command: ");
        /* Print just the command name */
        for (int i = 0; i < cmd_len; i++)
            term_putchar(td, cmd[i]);
        term_puts(td, "\nType 'help' for available commands.\n");
    }
}

/* ── AppContent callbacks ────────────────────────────────────── */

static void term_render(AppContent *self, Rect cr)
{
    TermData *td = (TermData *)self->data;
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
    if (self->data) kfree(self->data);
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
