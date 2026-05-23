#include "event_bus.h"
#include "draw.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <algorithm>

// ── Clipboard ───────────────────────────────────────────────────

void Clipboard::copy(const std::string& mime, const std::string& data) {
    mime_ = mime;
    data_ = data;
    sync_to_host();
    if (bus_) {
        ClipboardChangedEvent e;
        e.mime_type = mime;
        bus_->publish(e);
    }
}

std::string Clipboard::paste() const {
    return data_;
}

void Clipboard::clear() {
    data_.clear();
    mime_ = "text/plain";
}

void Clipboard::sync_to_host() const {
    if (mime_ == "text/plain" && !data_.empty()) {
        SDL_SetClipboardText(data_.c_str());
    }
}

void Clipboard::sync_from_host() {
    if (SDL_HasClipboardText()) {
        char* text = SDL_GetClipboardText();
        if (text) {
            data_ = text;
            mime_ = "text/plain";
            SDL_free(text);
        }
    }
}

// ── Notification Manager ────────────────────────────────────────

void NotificationManager::notify(const std::string& title, const std::string& body,
                                  const std::string& source_app_id,
                                  NotifyUrgency urgency) {
    ToastNotification toast;
    toast.id = next_id_++;
    toast.title = title;
    toast.body = body;
    toast.source_app_id = source_app_id;
    toast.urgency = urgency;
    toast.show_time = SDL_GetTicks();

    toasts_.push_back(toast);
    history_.push_back(toast);
    unread_++;

    if (bus_) {
        NotificationEvent e;
        e.title = title;
        e.body = body;
        e.source_app_id = source_app_id;
        e.urgency = urgency;
        e.id = toast.id;
        bus_->publish(e);
    }

    fprintf(stderr, "Notification: [%s] %s\n", title.c_str(), body.c_str());
}

void NotificationManager::tick(uint32_t now) {
    // Expire old toasts
    toasts_.erase(
        std::remove_if(toasts_.begin(), toasts_.end(),
                       [now, this](const ToastNotification& t) {
                           if (t.dismissed) return true;
                           if (t.urgency == NotifyUrgency::Critical) return false;
                           return (now - t.show_time) > TOAST_DURATION_MS;
                       }),
        toasts_.end());
}

void NotificationManager::render(SDL_Renderer* r, const Fonts* fonts, int screen_w) {
    if (toasts_.empty()) return;

    int toast_w = 280;
    int toast_h = 52;
    int gap = 8;
    int x = screen_w - toast_w - 16;
    int y = 44; // below topbar

    for (auto& toast : toasts_) {
        // Background
        SDL_Rect rect = {x, y, toast_w, toast_h};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        draw::filled_rounded_rect(r, rect, 8, {15, 20, 35, 210});
        draw::rounded_rect(r, rect, 8, {180, 195, 220, 40});

        // Urgency indicator
        SDL_Color indicator = {100, 150, 255, 200};
        if (toast.urgency == NotifyUrgency::Critical)
            indicator = {255, 80, 80, 200};
        draw::filled_rounded_rect(r, {x, y, 3, toast_h}, 1, indicator);

        // Title
        draw::text(r, fonts->body, toast.title.c_str(),
                   x + 12, y + 8, {230, 230, 240, 255});

        // Body
        draw::text(r, fonts->small, toast.body.c_str(),
                   x + 12, y + 28, {150, 160, 180, 255});

        y += toast_h + gap;
    }
}
