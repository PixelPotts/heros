#pragma once
#include "draw.h"
#include "audio.h"
#include "network.h"
#include "event_bus.h"
#include <string>
#include <cstdint>
#include <ctime>

// ── System Tray — unified topbar status area ────────────────────
// Manages and renders all topbar status icons with click-to-expand panels

class SystemTray {
public:
    void init(AudioManager* audio, NetworkManager* network,
              NotificationManager* notif);

    // Per-frame update
    void tick(uint32_t now);

    // Render the tray icons in the topbar (right side)
    void render(SDL_Renderer* r, const Fonts* fonts, int screen_w, int topbar_h);

    // Event handling — returns true if consumed
    bool handle_click(int mx, int my, int screen_w, int topbar_h);
    void on_mouse_move(int mx, int my, int screen_w, int topbar_h);

    // Render any open panels (call after main UI)
    void render_panels(SDL_Renderer* r, const Fonts* fonts, int screen_w);

    // Close all panels
    void close_all() { active_panel_ = -1; }

    // Query which panel is active (for power menu delegation)
    int active_panel() const { return active_panel_; }
    void clear_panel() { active_panel_ = -1; }

private:
    AudioManager* audio_ = nullptr;
    NetworkManager* network_ = nullptr;
    NotificationManager* notif_ = nullptr;

    int active_panel_ = -1; // -1=none, 0=volume, 1=network, 2=battery, 3=notifications
    int hover_icon_ = -1;

    // Battery simulation
    int battery_pct_ = 85;
    bool battery_charging_ = true;

    // Layout
    static const int ICON_SPACING = 30;
    static const int NUM_ICONS = 5; // volume, network, battery, bell, power

    struct IconInfo {
        Icon icon;
        int x, y;
        int w, h;
    };
    IconInfo icons_[NUM_ICONS];
    void compute_icon_positions(int screen_w, int topbar_h);

    // Individual panel renderers
    void render_volume_panel(SDL_Renderer* r, const Fonts* fonts, int screen_w);
    void render_battery_panel(SDL_Renderer* r, const Fonts* fonts, int screen_w);
    void render_notification_panel(SDL_Renderer* r, const Fonts* fonts, int screen_w);

    // Volume panel click handling
    bool handle_volume_click(int mx, int my, int screen_w);
};
