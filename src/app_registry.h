#pragma once
#include "window.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// ── App categories for launcher grouping ────────────────────────

enum class AppCategory {
    Productivity,
    System,
    Utility,
    Creative,
    Communication
};

// ── App manifest — describes an installed app ───────────────────

struct AppManifest {
    std::string app_id;         // unique identifier e.g. "com.heros.journal"
    std::string name;           // display name e.g. "Journal"
    Icon icon = Icon::Box;      // dock/launcher icon
    AppCategory category = AppCategory::Productivity;
    std::string version = "0.1.0";

    // Window hints
    SDL_Rect default_rect = {200, 70, 800, 500};
    int min_w = 200, min_h = 150;
    int max_w = 0, max_h = 0;   // 0 = no limit
    uint32_t default_flags = WF_Default;

    // Instance policy
    bool singleton = true;       // only one instance allowed

    // Launch behavior
    bool start_maximized = false;
    bool start_centered = true;
    bool dock_pinned = false;    // show in dock even when not running
    bool autostart = false;      // launch on OS startup
    int dock_order = 99;         // sort order in dock (lower = left)
};

// ── Factory type ────────────────────────────────────────────────

using AppFactory = std::function<std::unique_ptr<AppContent>()>;

// ── App Registry — single source of truth for installed apps ────

class AppRegistry {
public:
    // Registration
    bool register_app(const AppManifest& manifest, AppFactory factory);

    // Queries
    const AppManifest* get_manifest(const std::string& app_id) const;
    std::vector<const AppManifest*> list_apps() const;
    std::vector<const AppManifest*> list_by_category(AppCategory cat) const;
    std::vector<const AppManifest*> list_pinned_dock_apps() const;
    bool has_app(const std::string& app_id) const;

    // Factory
    std::unique_ptr<AppContent> create(const std::string& app_id) const;

    // Launch — creates process + window via WindowManager
    // Returns window id, or -1 on failure.
    // If singleton and already running, focuses existing window instead.
    int launch(const std::string& app_id, WindowManager& wm,
               int screen_w, int screen_h);

    // Query running state
    bool is_running(const std::string& app_id) const;
    int find_window_for_app(const std::string& app_id) const;

    // Track running apps (called by WM or main loop)
    void on_window_opened(const std::string& app_id, int window_id);
    void on_window_closed(int window_id);

private:
    struct AppEntry {
        AppManifest manifest;
        AppFactory factory;
    };

    struct RunningInstance {
        std::string app_id;
        int window_id;
    };

    std::unordered_map<std::string, AppEntry> apps_;
    std::vector<RunningInstance> running_;
};

// ── Built-in app registration ───────────────────────────────────

void register_builtin_apps(AppRegistry& registry);
