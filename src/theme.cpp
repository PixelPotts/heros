#include "theme.h"
#include <algorithm>

ThemeManager::ThemeManager() {
    register_defaults();
}

void ThemeManager::set_theme(const std::string& name) {
    for (int i = 0; i < (int)themes_.size(); i++) {
        if (themes_[i].name == name) {
            active_ = i;
            return;
        }
    }
}

void ThemeManager::register_defaults() {
    // ── Dark theme (default) ──────────────────────────────────
    {
        Theme t;
        t.name = "Midnight";
        t.colors.bg_primary     = {10, 12, 25, 255};
        t.colors.bg_card        = {22, 27, 55, 200};
        t.colors.bg_hover       = {100, 150, 255, 20};
        t.colors.bg_selected    = {100, 150, 255, 30};

        t.colors.text_primary   = {230, 230, 240, 255};
        t.colors.text_secondary = {150, 160, 180, 255};
        t.colors.text_disabled  = {80, 85, 100, 255};

        t.colors.accent         = {100, 150, 255, 255};
        t.colors.accent_dim     = {100, 150, 255, 40};

        t.colors.success        = {80, 200, 120, 255};
        t.colors.warning        = {230, 200, 60, 255};
        t.colors.error          = {220, 80, 80, 255};
        t.colors.info           = {100, 180, 255, 255};

        t.colors.border         = {180, 195, 220, 30};
        t.colors.divider        = {255, 255, 255, 20};
        t.colors.panel_tint     = {12, 15, 28, 150};

        themes_.push_back(t);
    }

    // ── Dawn theme ────────────────────────────────────────────
    {
        Theme t;
        t.name = "Dawn";
        t.colors.bg_primary     = {25, 15, 12, 255};
        t.colors.bg_card        = {45, 30, 28, 200};
        t.colors.bg_hover       = {255, 180, 100, 20};
        t.colors.bg_selected    = {255, 180, 100, 30};

        t.colors.text_primary   = {240, 230, 220, 255};
        t.colors.text_secondary = {180, 160, 140, 255};
        t.colors.text_disabled  = {100, 85, 75, 255};

        t.colors.accent         = {255, 160, 80, 255};
        t.colors.accent_dim     = {255, 160, 80, 40};

        t.colors.success        = {120, 200, 80, 255};
        t.colors.warning        = {230, 200, 60, 255};
        t.colors.error          = {220, 80, 80, 255};
        t.colors.info           = {100, 180, 255, 255};

        t.colors.border         = {220, 195, 170, 30};
        t.colors.divider        = {255, 240, 220, 20};
        t.colors.panel_tint     = {28, 18, 15, 150};

        themes_.push_back(t);
    }

    // ── Verdant theme ─────────────────────────────────────────
    {
        Theme t;
        t.name = "Verdant";
        t.colors.bg_primary     = {8, 20, 12, 255};
        t.colors.bg_card        = {18, 40, 28, 200};
        t.colors.bg_hover       = {80, 220, 140, 20};
        t.colors.bg_selected    = {80, 220, 140, 30};

        t.colors.text_primary   = {220, 240, 225, 255};
        t.colors.text_secondary = {140, 175, 155, 255};
        t.colors.text_disabled  = {70, 95, 80, 255};

        t.colors.accent         = {80, 220, 140, 255};
        t.colors.accent_dim     = {80, 220, 140, 40};

        t.colors.success        = {80, 220, 140, 255};
        t.colors.warning        = {230, 200, 60, 255};
        t.colors.error          = {220, 80, 80, 255};
        t.colors.info           = {100, 180, 255, 255};

        t.colors.border         = {140, 210, 170, 30};
        t.colors.divider        = {200, 255, 220, 20};
        t.colors.panel_tint     = {10, 22, 15, 150};

        themes_.push_back(t);
    }

    // ── Amethyst theme ────────────────────────────────────────
    {
        Theme t;
        t.name = "Amethyst";
        t.colors.bg_primary     = {18, 10, 28, 255};
        t.colors.bg_card        = {35, 22, 55, 200};
        t.colors.bg_hover       = {180, 100, 255, 20};
        t.colors.bg_selected    = {180, 100, 255, 30};

        t.colors.text_primary   = {235, 225, 245, 255};
        t.colors.text_secondary = {165, 150, 185, 255};
        t.colors.text_disabled  = {90, 75, 110, 255};

        t.colors.accent         = {180, 100, 255, 255};
        t.colors.accent_dim     = {180, 100, 255, 40};

        t.colors.success        = {80, 200, 120, 255};
        t.colors.warning        = {230, 200, 60, 255};
        t.colors.error          = {220, 80, 80, 255};
        t.colors.info           = {140, 160, 255, 255};

        t.colors.border         = {200, 170, 240, 30};
        t.colors.divider        = {240, 220, 255, 20};
        t.colors.panel_tint     = {20, 12, 30, 150};

        themes_.push_back(t);
    }
}
