#pragma once
#include "draw.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <typeindex>
#include <any>

// ── Event base ──────────────────────────────────────────────────

struct Event {
    virtual ~Event() = default;
};

// ── Core system events ──────────────────────────────────────────

struct AppLaunchedEvent : Event {
    std::string app_id;
    int window_id;
    uint32_t pid;
};

struct AppClosedEvent : Event {
    std::string app_id;
    int window_id;
    uint32_t pid;
};

struct FileChangedEvent : Event {
    std::string path;
    enum Action { Created, Modified, Deleted } action;
};

struct SettingChangedEvent : Event {
    std::string key;
    std::string value;
    std::string namespace_id; // "system" or app_id
};

struct ScreenResizedEvent : Event {
    int width, height;
};

// ── Notification event ──────────────────────────────────────────

enum class NotifyUrgency { Low, Normal, Critical };

struct NotificationEvent : Event {
    std::string title;
    std::string body;
    std::string source_app_id;
    NotifyUrgency urgency = NotifyUrgency::Normal;
    uint32_t id = 0; // auto-assigned
};

// ── Clipboard ───────────────────────────────────────────────────

struct ClipboardChangedEvent : Event {
    std::string mime_type;
};

// ── Event Bus ───────────────────────────────────────────────────

using SubscriptionId = uint32_t;

class EventBus {
public:
    // Subscribe to events of a specific type
    template<typename T>
    SubscriptionId subscribe(std::function<void(const T&)> callback) {
        SubscriptionId id = next_id_++;
        auto wrapper = [callback](const Event& e) {
            callback(static_cast<const T&>(e));
        };
        subs_.push_back({id, std::type_index(typeid(T)), wrapper});
        return id;
    }

    void unsubscribe(SubscriptionId id) {
        for (auto it = subs_.begin(); it != subs_.end(); ++it) {
            if (it->id == id) { subs_.erase(it); return; }
        }
    }

    // Publish an event to all subscribers of its type
    template<typename T>
    void publish(const T& event) {
        auto ti = std::type_index(typeid(T));
        for (auto& sub : subs_) {
            if (sub.type == ti) {
                sub.callback(event);
            }
        }
    }

private:
    struct Subscription {
        SubscriptionId id;
        std::type_index type;
        std::function<void(const Event&)> callback;
    };

    std::vector<Subscription> subs_;
    SubscriptionId next_id_ = 1;
};

// ── Clipboard Manager ───────────────────────────────────────────

class Clipboard {
public:
    void copy(const std::string& mime, const std::string& data);
    std::string paste() const;
    std::string mime() const { return mime_; }
    bool has_content() const { return !data_.empty(); }
    void clear();

    // Sync with SDL/host clipboard
    void sync_to_host() const;
    void sync_from_host();

    void set_event_bus(EventBus* bus) { bus_ = bus; }

private:
    std::string mime_ = "text/plain";
    std::string data_;
    EventBus* bus_ = nullptr;
};

// ── Notification Manager ────────────────────────────────────────

struct ToastNotification {
    uint32_t id;
    std::string title;
    std::string body;
    std::string source_app_id;
    NotifyUrgency urgency;
    uint32_t show_time;  // SDL ticks when shown
    bool dismissed = false;
};

class NotificationManager {
public:
    void notify(const std::string& title, const std::string& body,
                const std::string& source_app_id,
                NotifyUrgency urgency = NotifyUrgency::Normal);

    // Call each frame to update/expire toasts
    void tick(uint32_t now);

    // Render active toasts
    struct RenderCtx; // forward
    void render(SDL_Renderer* r, const struct Fonts* fonts, int screen_w);

    // Query
    int unread_count() const { return unread_; }
    const std::vector<ToastNotification>& active_toasts() const { return toasts_; }
    const std::vector<ToastNotification>& history() const { return history_; }

    void set_event_bus(EventBus* bus) { bus_ = bus; }

private:
    std::vector<ToastNotification> toasts_;
    std::vector<ToastNotification> history_;
    uint32_t next_id_ = 1;
    int unread_ = 0;
    EventBus* bus_ = nullptr;
    static const uint32_t TOAST_DURATION_MS = 4000;
};
