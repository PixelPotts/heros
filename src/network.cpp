#include "network.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <sstream>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};
static const SDL_Color GREEN  = {100, 200, 160, 200};

static const int PANEL_W = 260;
static const int PANEL_X_OFFSET = 80; // from right edge
static const int PANEL_Y = 38;
static const int ITEM_H = 36;
static const int PANEL_PAD = 8;

// ── Init ────────────────────────────────────────────────────────

void NetworkManager::init() {
    try_read_system_info();
    scan();
}

// ── Try to read real system network info ────────────────────────

void NetworkManager::try_read_system_info() {
    // Try to read real IP from system
    FILE* pipe = popen("hostname -I 2>/dev/null | awk '{print $1}'", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            // Trim newline
            char* nl = strchr(buf, '\n');
            if (nl) *nl = '\0';
            if (strlen(buf) > 0) {
                ip_address_ = buf;
                status_ = NetStatus::Connected;
            }
        }
        pclose(pipe);
    }

    // Try to get WiFi SSID
    pipe = popen("iwgetid -r 2>/dev/null || nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes' | cut -d: -f2", "r");
    if (pipe) {
        char buf[128] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            char* nl = strchr(buf, '\n');
            if (nl) *nl = '\0';
            if (strlen(buf) > 0) {
                connected_ssid_ = buf;
                signal_strength_ = 75; // default
            }
        }
        pclose(pipe);
    }

    // Try to read signal strength
    std::ifstream proc_wifi("/proc/net/wireless");
    if (proc_wifi.is_open()) {
        std::string line;
        while (std::getline(proc_wifi, line)) {
            if (line.find("wl") != std::string::npos || line.find("wlan") != std::string::npos) {
                // Format: iface status link level noise ...
                float level = 0;
                if (sscanf(line.c_str() + line.find(':') + 1, "%*f %f", &level) == 1) {
                    // Convert dBm-ish to percentage
                    signal_strength_ = std::max(0, std::min(100, (int)(level * 100 / 70)));
                }
            }
        }
    }

    if (status_ == NetStatus::Connected && connected_ssid_.empty()) {
        connected_ssid_ = "Ethernet";
    }
}

// ── Scan ────────────────────────────────────────────────────────

void NetworkManager::scan() {
    networks_.clear();

    // If connected, add the current network first
    if (!connected_ssid_.empty()) {
        networks_.push_back({connected_ssid_, signal_strength_, true, true});
    }

    // Add some simulated nearby networks for the UI
    networks_.push_back({"HerOS-Guest", 65, true, false});
    networks_.push_back({"Neighbor-5G", 42, true, false});
    networks_.push_back({"CafeWiFi", 30, false, false});
    networks_.push_back({"IoT-Network", 25, true, false});
}

void NetworkManager::tick(uint32_t now) {
    // Periodic re-scan every 30 seconds
    if (now - last_scan_ > 30000) {
        try_read_system_info();
        last_scan_ = now;
    }
}

void NetworkManager::connect(const std::string& ssid) {
    // Simulate connection
    for (auto& n : networks_) {
        n.connected = (n.ssid == ssid);
    }
    connected_ssid_ = ssid;
    status_ = NetStatus::Connected;
    signal_strength_ = 80;
}

void NetworkManager::disconnect() {
    for (auto& n : networks_) n.connected = false;
    connected_ssid_.clear();
    status_ = NetStatus::Disconnected;
    signal_strength_ = 0;
}

// ── Panel interaction ───────────────────────────────────────────

bool NetworkManager::handle_click(int mx, int my, int screen_w) {
    if (!panel_open_) return false;

    int px = screen_w - PANEL_W - PANEL_X_OFFSET;
    int py = PANEL_Y;
    int ph = PANEL_PAD * 2 + (int)networks_.size() * ITEM_H + 60; // header + networks + footer

    if (mx < px || mx >= px + PANEL_W || my < py || my >= py + ph) {
        panel_open_ = false;
        return false;
    }

    // Check which network was clicked
    int item_y = py + 40; // after header
    for (int i = 0; i < (int)networks_.size(); i++) {
        if (my >= item_y && my < item_y + ITEM_H) {
            if (!networks_[i].connected) {
                connect(networks_[i].ssid);
            }
            return true;
        }
        item_y += ITEM_H;
    }

    return true;
}

void NetworkManager::on_mouse_move(int mx, int my, int screen_w) {
    if (!panel_open_) { hover_index_ = -1; return; }

    int px = screen_w - PANEL_W - PANEL_X_OFFSET;
    int item_y = PANEL_Y + 40;
    hover_index_ = -1;

    for (int i = 0; i < (int)networks_.size(); i++) {
        if (mx >= px && mx < px + PANEL_W && my >= item_y && my < item_y + ITEM_H) {
            hover_index_ = i;
            break;
        }
        item_y += ITEM_H;
    }
}

// ── Render icon (for topbar) ────────────────────────────────────

void NetworkManager::render_icon(SDL_Renderer* r, int x, int y, int sz) const {
    SDL_Color col = (status_ == NetStatus::Connected) ? GREEN : DIM;

    // WiFi arc icon
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);

    int bars = 0;
    if (signal_strength_ > 20) bars = 1;
    if (signal_strength_ > 45) bars = 2;
    if (signal_strength_ > 70) bars = 3;

    // Draw arcs from bottom center
    int bx = x, by = y + sz / 4;
    draw::filled_circle(r, bx, by, 2, col);

    for (int i = 1; i <= 3; i++) {
        int ar = sz / 5 * i;
        SDL_Color ac = (i <= bars) ? col : SDL_Color{80, 90, 110, 100};
        for (float a = -(float)M_PI * 0.7f; a < -(float)M_PI * 0.3f; a += 0.05f) {
            SDL_SetRenderDrawColor(r, ac.r, ac.g, ac.b, ac.a);
            SDL_RenderDrawPoint(r, bx + (int)(ar * cosf(a)), by + (int)(ar * sinf(a)));
        }
    }
}

// ── Render panel ────────────────────────────────────────────────

void NetworkManager::render_panel(SDL_Renderer* r, const struct Fonts* fonts, int screen_w) {
    if (!panel_open_) return;

    int px = screen_w - PANEL_W - PANEL_X_OFFSET;
    int py = PANEL_Y;
    int ph = PANEL_PAD * 2 + (int)networks_.size() * ITEM_H + 70;

    // Panel background
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Rect panel = {px, py, PANEL_W, ph};
    draw::filled_rounded_rect(r, panel, 10, {15, 18, 30, 220});
    draw::rounded_rect(r, panel, 10, {180, 195, 220, 40});

    // Header
    draw::text(r, fonts->title, "Network", px + 14, py + 10, WHITE);

    // Status indicator
    const char* status_str = "Disconnected";
    SDL_Color status_col = DIM;
    if (status_ == NetStatus::Connected) {
        status_str = "Connected";
        status_col = GREEN;
    } else if (status_ == NetStatus::Connecting) {
        status_str = "Connecting...";
        status_col = ACCENT;
    }
    draw::text_right(r, fonts->small, status_str, px + PANEL_W - 14, py + 14, status_col);

    // Separator
    draw::line(r, px + 10, py + 35, px + PANEL_W - 10, py + 35, {180, 195, 220, 30});

    // Network list
    int item_y = py + 40;
    for (int i = 0; i < (int)networks_.size(); i++) {
        auto& n = networks_[i];
        SDL_Rect item = {px + 6, item_y, PANEL_W - 12, ITEM_H};

        if (i == hover_index_) {
            draw::filled_rounded_rect(r, item, 6, {100, 150, 255, 25});
        }

        // WiFi strength bars
        int bars = 0;
        if (n.signal_strength > 20) bars = 1;
        if (n.signal_strength > 45) bars = 2;
        if (n.signal_strength > 70) bars = 3;

        int bx = px + 24;
        int by = item_y + ITEM_H / 2;
        for (int b = 0; b < 3; b++) {
            int bh = 4 + b * 4;
            SDL_Color bc = (b < bars) ? (n.connected ? GREEN : DIM) : SDL_Color{60, 65, 80, 100};
            SDL_Rect bar = {bx + b * 5, by + 6 - bh, 3, bh};
            SDL_SetRenderDrawColor(r, bc.r, bc.g, bc.b, bc.a);
            SDL_RenderFillRect(r, &bar);
        }

        // SSID
        SDL_Color text_col = n.connected ? WHITE : DIM;
        draw::text(r, fonts->body, n.ssid.c_str(), px + 44, item_y + 10, text_col);

        // Lock icon if secured
        if (n.secured) {
            draw::icon(r, Icon::Lock, px + PANEL_W - 40, by, 12, {120, 130, 150, 150});
        }

        // Connected check
        if (n.connected) {
            draw::icon(r, Icon::Check, px + PANEL_W - 22, by, 12, GREEN);
        }

        item_y += ITEM_H;
    }

    // Footer: IP address
    if (!ip_address_.empty()) {
        draw::line(r, px + 10, item_y + 4, px + PANEL_W - 10, item_y + 4, {180, 195, 220, 30});
        char ip_str[64];
        snprintf(ip_str, sizeof(ip_str), "IP: %s", ip_address_.c_str());
        draw::text(r, fonts->small, ip_str, px + 14, item_y + 12, DIM);
    }
}
