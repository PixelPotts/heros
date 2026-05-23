#include "systray.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};
static const SDL_Color GREEN  = {100, 200, 160, 200};

static const int PANEL_W = 220;
static const int PANEL_Y = 38;

// ── Init ────────────────────────────────────────────────────────

void SystemTray::init(AudioManager* audio, NetworkManager* network,
                      NotificationManager* notif) {
    audio_ = audio;
    network_ = network;
    notif_ = notif;
}

void SystemTray::tick(uint32_t /*now*/) {
    // Battery simulation: slowly drain or charge
    if (battery_charging_) {
        if (battery_pct_ < 100) battery_pct_++;
    }
}

// ── Icon positions ──────────────────────────────────────────────

void SystemTray::compute_icon_positions(int screen_w, int topbar_h) {
    int ix = screen_w - 24;
    int cy = topbar_h / 2;
    int sz = 16;
    int hit_w = ICON_SPACING;

    // Right to left: power, bell, battery, network, volume
    Icon icon_types[] = {Icon::Power, Icon::Bell, Icon::Target, Icon::Waveform, Icon::Volume};
    for (int i = 0; i < NUM_ICONS; i++) {
        icons_[i].icon = icon_types[i];
        icons_[i].x = ix;
        icons_[i].y = cy;
        icons_[i].w = hit_w;
        icons_[i].h = topbar_h;
        ix -= ICON_SPACING;
    }
    (void)sz;
}

// ── Render tray icons ───────────────────────────────────────────

void SystemTray::render(SDL_Renderer* r, const Fonts* fonts, int screen_w, int topbar_h) {
    compute_icon_positions(screen_w, topbar_h);
    (void)fonts;

    for (int i = 0; i < NUM_ICONS; i++) {
        SDL_Color col = DIM;
        if (hover_icon_ == i || active_panel_ == i) {
            col = ACCENT;
        }

        int x = icons_[i].x;
        int y = icons_[i].y;

        switch (i) {
        case 0: // Power
            draw::icon(r, Icon::Power, x, y, 16, col);
            break;
        case 1: // Bell (notifications)
        {
            draw::icon(r, Icon::Bell, x, y, 16, col);
            // Unread badge
            if (notif_ && notif_->unread_count() > 0) {
                draw::filled_circle(r, x + 6, y - 6, 4, {255, 80, 80, 220});
                char buf[4];
                snprintf(buf, sizeof(buf), "%d", notif_->unread_count());
                draw::text_centered(r, nullptr, buf, x + 6, y - 10, WHITE);
            }
            break;
        }
        case 2: // Battery
        {
            // Battery icon: rect with fill level
            int bw = 14, bh = 8;
            int bx = x - bw / 2, by = y - bh / 2;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

            // Outline
            SDL_Rect outline = {bx, by, bw, bh};
            SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
            SDL_RenderDrawRect(r, &outline);
            // Nub
            SDL_Rect nub = {bx + bw, by + 2, 2, bh - 4};
            SDL_RenderFillRect(r, &nub);
            // Fill
            int fw = (bw - 2) * battery_pct_ / 100;
            SDL_Color fc = battery_pct_ > 20 ? GREEN : SDL_Color{255, 80, 80, 200};
            SDL_Rect fill = {bx + 1, by + 1, fw, bh - 2};
            SDL_SetRenderDrawColor(r, fc.r, fc.g, fc.b, fc.a);
            SDL_RenderFillRect(r, &fill);
            // Charging indicator
            if (battery_charging_) {
                draw::icon(r, Icon::Sparkle, x, y, 8, {255, 220, 100, 180});
            }
            break;
        }
        case 3: // Network
            if (network_) {
                network_->render_icon(r, x, y, 16);
            } else {
                draw::icon(r, Icon::Waveform, x, y, 16, col);
            }
            break;
        case 4: // Volume
        {
            if (audio_ && audio_->muted()) {
                // Muted: draw X over volume icon
                draw::icon(r, Icon::Volume, x, y, 16, {180, 80, 80, 200});
                SDL_SetRenderDrawColor(r, 180, 80, 80, 200);
                SDL_RenderDrawLine(r, x - 4, y - 4, x + 4, y + 4);
            } else {
                draw::icon(r, Icon::Volume, x, y, 16, col);
            }
            break;
        }
        }
    }

    // Clock in center-right area (before icons)
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t->tm_hour, t->tm_min);
    draw::text(r, fonts->body, time_str, screen_w - NUM_ICONS * ICON_SPACING - 70, 10, WHITE);
}

// ── Click handling ──────────────────────────────────────────────

bool SystemTray::handle_click(int mx, int my, int screen_w, int topbar_h) {
    compute_icon_positions(screen_w, topbar_h);

    // Check if clicking within an open panel
    if (active_panel_ == 4) {
        if (handle_volume_click(mx, my, screen_w)) return true;
    }
    if (active_panel_ == 3 && network_) {
        if (network_->handle_click(mx, my, screen_w)) return true;
    }

    // Check icon clicks
    if (my > topbar_h) {
        if (active_panel_ >= 0) { active_panel_ = -1; return true; }
        return false;
    }

    for (int i = 0; i < NUM_ICONS; i++) {
        int ix = icons_[i].x;
        if (mx >= ix - ICON_SPACING / 2 && mx < ix + ICON_SPACING / 2) {
            if (active_panel_ == i) {
                active_panel_ = -1; // toggle off
            } else {
                active_panel_ = i;
                if (i == 3 && network_) {
                    network_->toggle_panel();
                }
            }
            return true;
        }
    }

    if (active_panel_ >= 0) {
        active_panel_ = -1;
        return false;
    }
    return false;
}

void SystemTray::on_mouse_move(int mx, int my, int screen_w, int topbar_h) {
    compute_icon_positions(screen_w, topbar_h);
    hover_icon_ = -1;

    if (my <= topbar_h) {
        for (int i = 0; i < NUM_ICONS; i++) {
            int ix = icons_[i].x;
            if (mx >= ix - ICON_SPACING / 2 && mx < ix + ICON_SPACING / 2) {
                hover_icon_ = i;
                break;
            }
        }
    }

    if (active_panel_ == 3 && network_) {
        network_->on_mouse_move(mx, my, screen_w);
    }
}

// ── Render panels ───────────────────────────────────────────────

void SystemTray::render_panels(SDL_Renderer* r, const Fonts* fonts, int screen_w) {
    switch (active_panel_) {
    case 0: // Power — handled by power manager (feature 8)
        break;
    case 1: // Notifications
        render_notification_panel(r, fonts, screen_w);
        break;
    case 2: // Battery
        render_battery_panel(r, fonts, screen_w);
        break;
    case 3: // Network
        if (network_) network_->render_panel(r, fonts, screen_w);
        break;
    case 4: // Volume
        render_volume_panel(r, fonts, screen_w);
        break;
    }
}

// ── Volume panel ────────────────────────────────────────────────

void SystemTray::render_volume_panel(SDL_Renderer* r, const Fonts* fonts, int screen_w) {
    int px = screen_w - PANEL_W - 20;
    int py = PANEL_Y;
    int ph = 120;

    SDL_Rect panel = {px, py, PANEL_W, ph};
    draw::filled_rounded_rect(r, panel, 10, {15, 18, 30, 220});
    draw::rounded_rect(r, panel, 10, {180, 195, 220, 40});

    draw::text(r, fonts->title, "Volume", px + 14, py + 10, WHITE);

    // Mute toggle
    const char* mute_str = (audio_ && audio_->muted()) ? "Unmute" : "Mute";
    draw::text_right(r, fonts->small, mute_str, px + PANEL_W - 14, py + 14, ACCENT);

    // Volume slider track
    int track_y = py + 55;
    int track_x = px + 20;
    int track_w = PANEL_W - 40;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Rect track = {track_x, track_y, track_w, 6};
    draw::filled_rounded_rect(r, track, 3, {40, 45, 60, 200});

    // Fill based on volume
    float vol = audio_ ? audio_->volume() : 0.7f;
    int fill_w = (int)(track_w * vol);
    SDL_Rect fill = {track_x, track_y, fill_w, 6};
    draw::filled_rounded_rect(r, fill, 3, {100, 150, 255, 200});

    // Thumb
    int thumb_x = track_x + fill_w;
    draw::filled_circle(r, thumb_x, track_y + 3, 8, ACCENT);

    // Percentage
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", (int)(vol * 100));
    draw::text_centered(r, fonts->body, pct, px + PANEL_W / 2, py + 80, WHITE);

    // Sound labels
    draw::text(r, fonts->small, "System Sounds", px + 14, py + 98, DIM);
    draw::text_right(r, fonts->small, audio_ && audio_->muted() ? "Off" : "On",
                     px + PANEL_W - 14, py + 98, audio_ && audio_->muted() ? DIM : GREEN);
}

bool SystemTray::handle_volume_click(int mx, int my, int screen_w) {
    int px = screen_w - PANEL_W - 20;
    int py = PANEL_Y;
    int ph = 120;

    if (mx < px || mx >= px + PANEL_W || my < py || my >= py + ph)
        return false;

    // Check mute toggle (top right area)
    if (my < py + 35 && mx > px + PANEL_W - 60) {
        if (audio_) audio_->toggle_mute();
        return true;
    }

    // Check volume slider
    int track_y = py + 55;
    int track_x = px + 20;
    int track_w = PANEL_W - 40;

    if (my >= track_y - 12 && my <= track_y + 18 &&
        mx >= track_x && mx <= track_x + track_w) {
        float vol = (float)(mx - track_x) / track_w;
        if (audio_) audio_->set_volume(vol);
        return true;
    }

    return true;
}

// ── Battery panel ───────────────────────────────────────────────

void SystemTray::render_battery_panel(SDL_Renderer* r, const Fonts* fonts, int screen_w) {
    int px = screen_w - PANEL_W - 50;
    int py = PANEL_Y;
    int ph = 100;

    SDL_Rect panel = {px, py, PANEL_W, ph};
    draw::filled_rounded_rect(r, panel, 10, {15, 18, 30, 220});
    draw::rounded_rect(r, panel, 10, {180, 195, 220, 40});

    draw::text(r, fonts->title, "Battery", px + 14, py + 10, WHITE);

    // Large percentage
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", battery_pct_);
    draw::text(r, fonts->widget, pct, px + 14, py + 38, WHITE);

    // Status
    const char* status = battery_charging_ ? "Charging" : "On Battery";
    draw::text(r, fonts->small, status, px + 14, py + 72, battery_charging_ ? GREEN : DIM);

    // Bar
    int bar_x = px + 90, bar_y = py + 50;
    int bar_w = PANEL_W - 110, bar_h = 12;
    SDL_Rect bar_bg = {bar_x, bar_y, bar_w, bar_h};
    draw::filled_rounded_rect(r, bar_bg, 4, {40, 45, 60, 200});

    int fw = bar_w * battery_pct_ / 100;
    SDL_Color fc = battery_pct_ > 20 ? GREEN : SDL_Color{255, 80, 80, 200};
    SDL_Rect bar_fill = {bar_x, bar_y, fw, bar_h};
    draw::filled_rounded_rect(r, bar_fill, 4, fc);
}

// ── Notification panel ──────────────────────────────────────────

void SystemTray::render_notification_panel(SDL_Renderer* r, const Fonts* fonts, int screen_w) {
    int px = screen_w - PANEL_W - 35;
    int py = PANEL_Y;

    auto& hist = notif_ ? notif_->history() : *(const std::vector<ToastNotification>*)nullptr;
    int count = notif_ ? std::min((int)hist.size(), 5) : 0;
    int ph = 40 + count * 50 + (count == 0 ? 30 : 0);

    SDL_Rect panel = {px, py, PANEL_W, ph};
    draw::filled_rounded_rect(r, panel, 10, {15, 18, 30, 220});
    draw::rounded_rect(r, panel, 10, {180, 195, 220, 40});

    draw::text(r, fonts->title, "Notifications", px + 14, py + 10, WHITE);

    if (count == 0) {
        draw::text_centered(r, fonts->body, "No notifications", px + PANEL_W / 2, py + 45, DIM);
    } else {
        int item_y = py + 36;
        for (int i = count - 1; i >= 0; i--) {
            auto& n = hist[i];
            draw::text(r, fonts->body, n.title.c_str(), px + 14, item_y + 4, WHITE);
            // Truncate body
            std::string body = n.body;
            if (body.size() > 30) body = body.substr(0, 27) + "...";
            draw::text(r, fonts->small, body.c_str(), px + 14, item_y + 22, DIM);
            draw::line(r, px + 10, item_y + 45, px + PANEL_W - 10, item_y + 45, {180, 195, 220, 20});
            item_y += 50;
        }
    }
}
