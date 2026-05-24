#ifndef UI_THEME_H
#define UI_THEME_H

#include "types.h"

typedef struct {
    /* Background gradient */
    Color bg_top;
    Color bg_bottom;

    /* Panel colors */
    Color panel_bg;         /* semi-transparent panel background */
    Color panel_border;
    Color panel_highlight;

    /* Text colors */
    Color text_primary;
    Color text_secondary;
    Color text_muted;

    /* Window */
    Color window_bg;
    Color window_title_bg;
    Color window_border;
    Color window_shadow;

    /* Accent */
    Color accent;
    Color accent_hover;
    Color accent_dim;

    /* Status */
    Color success;
    Color warning;
    Color error;

    /* Dock/topbar */
    Color dock_bg;
    Color topbar_bg;
    Color sidebar_bg;

    /* Name */
    const char *name;
} ThemeColors;

typedef enum {
    THEME_MIDNIGHT = 0,
    THEME_DAWN,
    THEME_VERDANT,
    THEME_AMETHYST,
    THEME_COUNT
} ThemeId;

void          theme_init(void);
ThemeId       theme_current(void);
const ThemeColors *theme_colors(void);
void          theme_set(ThemeId id);
const char   *theme_name(ThemeId id);

#endif
