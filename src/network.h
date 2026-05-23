#pragma once
#include "draw.h"
#include <string>
#include <vector>
#include <cstdint>

// ── Network connection info ─────────────────────────────────────

enum class NetStatus {
    Disconnected,
    Connecting,
    Connected
};

struct WifiNetwork {
    std::string ssid;
    int signal_strength; // 0-100
    bool secured;
    bool connected;
};

// ── Network Manager ─────────────────────────────────────────────
// Simulated network manager that reads real system info when available

class NetworkManager {
public:
    void init();
    void tick(uint32_t now);

    // Status
    NetStatus status() const { return status_; }
    const std::string& connected_ssid() const { return connected_ssid_; }
    const std::string& ip_address() const { return ip_address_; }
    int signal_strength() const { return signal_strength_; }

    // WiFi networks (scanned or simulated)
    const std::vector<WifiNetwork>& networks() const { return networks_; }
    void scan();
    void connect(const std::string& ssid);
    void disconnect();

    // Panel state
    bool panel_open() const { return panel_open_; }
    void toggle_panel() { panel_open_ = !panel_open_; }
    void close_panel() { panel_open_ = false; }

    // Hit test for panel
    bool handle_click(int mx, int my, int screen_w);
    void on_mouse_move(int mx, int my, int screen_w);

    // Render the dropdown panel
    void render_panel(SDL_Renderer* r, const struct Fonts* fonts, int screen_w);

    // Render the topbar icon
    void render_icon(SDL_Renderer* r, int x, int y, int sz) const;

private:
    NetStatus status_ = NetStatus::Disconnected;
    std::string connected_ssid_;
    std::string ip_address_;
    int signal_strength_ = 0;
    std::vector<WifiNetwork> networks_;
    bool panel_open_ = false;
    int hover_index_ = -1;
    uint32_t last_scan_ = 0;

    void try_read_system_info();
};
