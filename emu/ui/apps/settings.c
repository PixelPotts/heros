#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../widgets.h"
#include "../window.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"

typedef struct {
    int selected_theme;
} SettingsData;

static void settings_render(AppContent *self, Rect cr)
{
    SettingsData *sd = (SettingsData *)self->data;
    const ThemeColors *tc = theme_colors();
    int mx, my;
    hal_input_mouse_pos(&mx, &my);

    int y = cr.y + 8;

    /* Title */
    draw_text(cr.x + 12, y, "Settings", tc->text_primary, FONT_SIZE_MEDIUM);
    y += 24;
    widget_separator(cr.x + 8, y, cr.w - 16);
    y += 12;

    /* Theme section */
    widget_section_header(cr.x + 12, y, "APPEARANCE");
    y += 18;

    draw_text(cr.x + 12, y, "Theme", tc->text_primary, FONT_SIZE_SMALL);
    y += 16;

    for (int i = 0; i < THEME_COUNT; i++) {
        Rect item = RECT(cr.x + 12, y, cr.w - 24, 28);
        int sel = (i == (int)theme_current());

        widget_list_item(item, theme_name(i), sel ? "Active" : "",
                         sel, mx, my);

        /* Color preview dots */
        ThemeId prev = theme_current();
        /* We need the theme colors for preview, but can't switch easily */
        /* Just draw accent-colored dot for active */
        if (sel) {
            widget_status_dot(cr.x + cr.w - 40, y + 14, 4, tc->accent);
        }

        y += 30;
        (void)prev;
    }

    y += 8;
    widget_separator(cr.x + 8, y, cr.w - 16);
    y += 12;

    /* System info */
    widget_section_header(cr.x + 12, y, "SYSTEM INFO");
    y += 18;

    draw_text(cr.x + 12, y, "OS: HerOS v1.0", tc->text_secondary, FONT_SIZE_SMALL);
    y += 14;
    draw_text(cr.x + 12, y, "Arch: RISC-V RV32IM", tc->text_secondary, FONT_SIZE_SMALL);
    y += 14;
    draw_text(cr.x + 12, y, "Display: 1280x720", tc->text_secondary, FONT_SIZE_SMALL);
    y += 14;

    char mem_str[64];
    strcpy(mem_str, "RAM: ");
    char buf[16];
    utoa((unsigned)(mm_total_pages() * 4 / 1024), buf, 10);
    strcat(mem_str, buf);
    strcat(mem_str, " MB total");
    draw_text(cr.x + 12, y, mem_str, tc->text_secondary, FONT_SIZE_SMALL);
}

static void settings_on_mouse_down(AppContent *self, int x, int y)
{
    (void)self; (void)x;

    /* Check if clicking a theme item */
    int base_y = 8 + 24 + 12 + 18 + 16;  /* approximate y offset to theme list */
    for (int i = 0; i < THEME_COUNT; i++) {
        int item_y = base_y + i * 30;
        if (y >= item_y && y < item_y + 28) {
            theme_set((ThemeId)i);
            break;
        }
    }
}

static void settings_destroy(AppContent *self)
{
    if (self->data) kfree(self->data);
    kfree(self);
}

AppContent *settings_create(void)
{
    AppContent *app = (AppContent *)kmalloc(sizeof(AppContent));
    if (!app) return (void *)0;
    memset(app, 0, sizeof(AppContent));

    SettingsData *sd = (SettingsData *)kmalloc(sizeof(SettingsData));
    if (!sd) { kfree(app); return (void *)0; }
    memset(sd, 0, sizeof(SettingsData));

    sd->selected_theme = (int)theme_current();

    app->render = settings_render;
    app->on_mouse_down = settings_on_mouse_down;
    app->destroy = settings_destroy;
    app->data = sd;

    return app;
}
