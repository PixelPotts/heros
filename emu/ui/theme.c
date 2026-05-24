#include "theme.h"

static const ThemeColors themes[THEME_COUNT] = {
    [THEME_MIDNIGHT] = {
        .bg_top      = {15, 15, 35, 255},
        .bg_bottom   = {5, 5, 20, 255},
        .panel_bg    = {25, 25, 50, 200},
        .panel_border = {60, 60, 100, 150},
        .panel_highlight = {80, 80, 140, 180},
        .text_primary   = {230, 230, 245, 255},
        .text_secondary = {160, 160, 190, 255},
        .text_muted     = {100, 100, 130, 255},
        .window_bg      = {20, 20, 40, 240},
        .window_title_bg = {30, 30, 60, 250},
        .window_border  = {50, 50, 90, 200},
        .window_shadow  = {0, 0, 0, 100},
        .accent         = {100, 130, 255, 255},
        .accent_hover   = {130, 160, 255, 255},
        .accent_dim     = {60, 80, 180, 255},
        .success        = {80, 200, 120, 255},
        .warning        = {240, 180, 40, 255},
        .error          = {220, 70, 70, 255},
        .dock_bg        = {15, 15, 35, 220},
        .topbar_bg      = {20, 20, 45, 230},
        .sidebar_bg     = {18, 18, 40, 210},
        .name           = "Midnight"
    },
    [THEME_DAWN] = {
        .bg_top      = {45, 30, 30, 255},
        .bg_bottom   = {25, 15, 15, 255},
        .panel_bg    = {55, 35, 35, 200},
        .panel_border = {100, 65, 65, 150},
        .panel_highlight = {130, 85, 85, 180},
        .text_primary   = {245, 230, 225, 255},
        .text_secondary = {200, 170, 160, 255},
        .text_muted     = {140, 110, 100, 255},
        .window_bg      = {45, 28, 28, 240},
        .window_title_bg = {60, 35, 35, 250},
        .window_border  = {90, 55, 55, 200},
        .window_shadow  = {0, 0, 0, 100},
        .accent         = {230, 120, 80, 255},
        .accent_hover   = {250, 150, 110, 255},
        .accent_dim     = {180, 90, 60, 255},
        .success        = {100, 190, 100, 255},
        .warning        = {230, 180, 50, 255},
        .error          = {200, 60, 60, 255},
        .dock_bg        = {40, 25, 25, 220},
        .topbar_bg      = {50, 30, 30, 230},
        .sidebar_bg     = {42, 27, 27, 210},
        .name           = "Dawn"
    },
    [THEME_VERDANT] = {
        .bg_top      = {15, 35, 25, 255},
        .bg_bottom   = {5, 20, 12, 255},
        .panel_bg    = {20, 45, 30, 200},
        .panel_border = {45, 90, 60, 150},
        .panel_highlight = {65, 120, 80, 180},
        .text_primary   = {225, 245, 230, 255},
        .text_secondary = {160, 195, 170, 255},
        .text_muted     = {100, 135, 110, 255},
        .window_bg      = {15, 35, 22, 240},
        .window_title_bg = {25, 50, 35, 250},
        .window_border  = {40, 80, 55, 200},
        .window_shadow  = {0, 0, 0, 100},
        .accent         = {70, 200, 120, 255},
        .accent_hover   = {100, 230, 150, 255},
        .accent_dim     = {50, 150, 90, 255},
        .success        = {80, 210, 120, 255},
        .warning        = {220, 190, 40, 255},
        .error          = {200, 70, 70, 255},
        .dock_bg        = {12, 30, 20, 220},
        .topbar_bg      = {18, 38, 26, 230},
        .sidebar_bg     = {14, 32, 22, 210},
        .name           = "Verdant"
    },
    [THEME_AMETHYST] = {
        .bg_top      = {30, 15, 40, 255},
        .bg_bottom   = {15, 8, 22, 255},
        .panel_bg    = {40, 22, 55, 200},
        .panel_border = {80, 50, 100, 150},
        .panel_highlight = {110, 70, 140, 180},
        .text_primary   = {240, 225, 250, 255},
        .text_secondary = {190, 165, 210, 255},
        .text_muted     = {130, 105, 150, 255},
        .window_bg      = {30, 18, 42, 240},
        .window_title_bg = {45, 25, 60, 250},
        .window_border  = {70, 45, 95, 200},
        .window_shadow  = {0, 0, 0, 100},
        .accent         = {170, 100, 255, 255},
        .accent_hover   = {200, 135, 255, 255},
        .accent_dim     = {120, 70, 200, 255},
        .success        = {100, 200, 130, 255},
        .warning        = {230, 180, 50, 255},
        .error          = {210, 65, 65, 255},
        .dock_bg        = {25, 12, 35, 220},
        .topbar_bg      = {32, 17, 44, 230},
        .sidebar_bg     = {28, 14, 38, 210},
        .name           = "Amethyst"
    }
};

static ThemeId current_theme = THEME_MIDNIGHT;

void theme_init(void)
{
    current_theme = THEME_MIDNIGHT;
}

ThemeId theme_current(void)
{
    return current_theme;
}

const ThemeColors *theme_colors(void)
{
    return &themes[current_theme];
}

void theme_set(ThemeId id)
{
    if (id < THEME_COUNT)
        current_theme = id;
}

const char *theme_name(ThemeId id)
{
    if (id < THEME_COUNT)
        return themes[id].name;
    return "Unknown";
}
