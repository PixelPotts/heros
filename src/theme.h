#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

// ── Color Palette ───────────────────────────────────────────────

struct ThemeColors {
    // Core surfaces
    SDL_Color bg_primary;       // main background tint
    SDL_Color bg_card;          // card/panel background
    SDL_Color bg_hover;         // hover state
    SDL_Color bg_selected;      // selected/active state

    // Text
    SDL_Color text_primary;     // main text
    SDL_Color text_secondary;   // dimmed/label text
    SDL_Color text_disabled;    // disabled text

    // Accent
    SDL_Color accent;           // primary accent (buttons, links)
    SDL_Color accent_dim;       // faded accent for backgrounds

    // Semantic
    SDL_Color success;          // green
    SDL_Color warning;          // yellow
    SDL_Color error;            // red
    SDL_Color info;             // blue

    // Borders & dividers
    SDL_Color border;           // subtle border
    SDL_Color divider;          // line dividers

    // Dock / panel tint
    SDL_Color panel_tint;       // frosted panel overlay
};

// ── Theme Definition ────────────────────────────────────────────

struct Theme {
    std::string name;
    ThemeColors colors;

    // Geometry
    int corner_radius = 8;
    int panel_radius = 10;
    int button_radius = 6;
    int titlebar_height = 32;
    int dock_height = 48;
    int sidebar_width = 180;
    int topbar_height = 36;

    // Effects
    bool frosted_glass = true;
    float blur_intensity = 1.0f;
    float panel_opacity = 0.6f;
};

// ── Theme Manager ───────────────────────────────────────────────

class ThemeManager {
public:
    ThemeManager();

    const Theme& current() const { return themes_[active_]; }
    const ThemeColors& colors() const { return themes_[active_].colors; }

    // Switch themes
    void set_theme(const std::string& name);
    int theme_count() const { return (int)themes_.size(); }
    const Theme& theme_at(int i) const { return themes_[i]; }
    int active_index() const { return active_; }

private:
    std::vector<Theme> themes_;
    int active_ = 0;

    void register_defaults();
};
