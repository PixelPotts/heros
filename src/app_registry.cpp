#include "app_registry.h"
#include "apps/journal_app.h"
#include "apps/finance_app.h"
#include <cstdio>
#include <algorithm>

// ── AppContext implementation ───────────────────────────────────

void AppContext::request_close() {
    if (wm) wm->close_window(window_id);
}

void AppContext::set_title(const std::string& title) {
    if (wm) {
        Window* w = wm->find_window(window_id);
        if (w) w->title = title;
    }
}

int AppContext::launch_app(const std::string& target_app_id) {
    if (registry && wm)
        return registry->launch(target_app_id, *wm, screen_w, screen_h);
    return -1;
}

// ── Registration ────────────────────────────────────────────────

bool AppRegistry::register_app(const AppManifest& manifest, AppFactory factory) {
    if (manifest.app_id.empty()) {
        fprintf(stderr, "AppRegistry: rejected app with empty app_id\n");
        return false;
    }
    if (manifest.name.empty()) {
        fprintf(stderr, "AppRegistry: rejected app '%s' with empty name\n",
                manifest.app_id.c_str());
        return false;
    }
    if (!factory) {
        fprintf(stderr, "AppRegistry: rejected app '%s' with null factory\n",
                manifest.app_id.c_str());
        return false;
    }
    if (apps_.count(manifest.app_id)) {
        fprintf(stderr, "AppRegistry: duplicate app_id '%s'\n",
                manifest.app_id.c_str());
        return false;
    }

    apps_[manifest.app_id] = {manifest, std::move(factory)};
    fprintf(stderr, "AppRegistry: registered '%s' (%s)\n",
            manifest.name.c_str(), manifest.app_id.c_str());
    return true;
}

// ── Queries ─────────────────────────────────────────────────────

const AppManifest* AppRegistry::get_manifest(const std::string& app_id) const {
    auto it = apps_.find(app_id);
    if (it == apps_.end()) return nullptr;
    return &it->second.manifest;
}

std::vector<const AppManifest*> AppRegistry::list_apps() const {
    std::vector<const AppManifest*> result;
    result.reserve(apps_.size());
    for (auto& [id, entry] : apps_)
        result.push_back(&entry.manifest);
    return result;
}

std::vector<const AppManifest*> AppRegistry::list_by_category(AppCategory cat) const {
    std::vector<const AppManifest*> result;
    for (auto& [id, entry] : apps_) {
        if (entry.manifest.category == cat)
            result.push_back(&entry.manifest);
    }
    return result;
}

std::vector<const AppManifest*> AppRegistry::list_pinned_dock_apps() const {
    std::vector<const AppManifest*> result;
    for (auto& [id, entry] : apps_) {
        if (entry.manifest.dock_pinned)
            result.push_back(&entry.manifest);
    }
    // Sort by dock_order
    std::sort(result.begin(), result.end(),
              [](const AppManifest* a, const AppManifest* b) {
                  return a->dock_order < b->dock_order;
              });
    return result;
}

bool AppRegistry::has_app(const std::string& app_id) const {
    return apps_.count(app_id) > 0;
}

// ── Factory ─────────────────────────────────────────────────────

std::unique_ptr<AppContent> AppRegistry::create(const std::string& app_id) const {
    auto it = apps_.find(app_id);
    if (it == apps_.end()) {
        fprintf(stderr, "AppRegistry: unknown app_id '%s'\n", app_id.c_str());
        return nullptr;
    }
    return it->second.factory();
}

// ── Launch ──────────────────────────────────────────────────────

int AppRegistry::launch(const std::string& app_id, WindowManager& wm,
                        int screen_w, int screen_h) {
    auto it = apps_.find(app_id);
    if (it == apps_.end()) {
        fprintf(stderr, "AppRegistry: cannot launch unknown app '%s'\n",
                app_id.c_str());
        return -1;
    }

    const auto& manifest = it->second.manifest;

    // Singleton check: if already running, focus existing
    if (manifest.singleton) {
        int existing = find_window_for_app(app_id);
        if (existing >= 0) {
            auto* win = wm.find_window(existing);
            if (win) {
                if (win->minimized) {
                    wm.restore_from_dock(existing, screen_w, screen_h);
                } else {
                    wm.bring_to_front(existing);
                    wm.set_focus(existing);
                }
            }
            return existing;
        }
    }

    // Create app content
    auto content = it->second.factory();
    if (!content) {
        fprintf(stderr, "AppRegistry: factory returned null for '%s'\n",
                app_id.c_str());
        return -1;
    }

    // Compute initial rect
    SDL_Rect rect = manifest.default_rect;
    if (manifest.start_centered) {
        rect.x = (screen_w - rect.w) / 2;
        rect.y = (screen_h - rect.h) / 2;
    }

    // Open window
    int win_id = wm.open_window(
        manifest.name,
        manifest.icon,
        rect,
        manifest.default_flags,
        std::move(content)
    );

    if (win_id < 0) {
        fprintf(stderr, "AppRegistry: failed to open window for '%s'\n",
                app_id.c_str());
        return -1;
    }

    // Apply min size + set app context
    auto* win = wm.find_window(win_id);
    if (win) {
        win->min_w = manifest.min_w;
        // Wire app context so the app can call back into the OS
        if (win->content) {
            AppContext actx;
            actx.window_id = win_id;
            actx.app_id = app_id;
            actx.wm = &wm;
            actx.registry = this;
            actx.screen_w = screen_w;
            actx.screen_h = screen_h;
            win->content->set_context(actx);
        }
        win->min_h = manifest.min_h;
    }

    // Apply start_maximized
    if (manifest.start_maximized) {
        wm.maximize(win_id, screen_w, screen_h);
    }

    // Track running instance
    on_window_opened(app_id, win_id);

    fprintf(stderr, "AppRegistry: launched '%s' (window %d)\n",
            manifest.name.c_str(), win_id);
    return win_id;
}

// ── Running state tracking ──────────────────────────────────────

bool AppRegistry::is_running(const std::string& app_id) const {
    for (auto& inst : running_) {
        if (inst.app_id == app_id) return true;
    }
    return false;
}

int AppRegistry::find_window_for_app(const std::string& app_id) const {
    for (auto& inst : running_) {
        if (inst.app_id == app_id) return inst.window_id;
    }
    return -1;
}

void AppRegistry::on_window_opened(const std::string& app_id, int window_id) {
    running_.push_back({app_id, window_id});
}

void AppRegistry::on_window_closed(int window_id) {
    running_.erase(
        std::remove_if(running_.begin(), running_.end(),
                       [window_id](const RunningInstance& inst) {
                           return inst.window_id == window_id;
                       }),
        running_.end()
    );
}

// ── Built-in app registration ───────────────────────────────────

void register_builtin_apps(AppRegistry& registry) {
    // Journal
    {
        AppManifest m;
        m.app_id = "com.heros.journal";
        m.name = "Journal";
        m.icon = Icon::Journal;
        m.category = AppCategory::Productivity;
        m.default_rect = {0, 0, 800, 550};
        m.min_w = 400;
        m.min_h = 300;
        m.singleton = true;
        m.start_maximized = false;
        m.start_centered = true;
        m.dock_pinned = true;
        m.dock_order = 4;   // match current dock position
        m.autostart = false;
        registry.register_app(m, []() {
            return std::make_unique<JournalApp>();
        });
    }

    // Finance
    {
        AppManifest m;
        m.app_id = "com.heros.finance";
        m.name = "Finance";
        m.icon = Icon::Briefcase;
        m.category = AppCategory::Productivity;
        m.default_rect = {0, 0, 900, 600};
        m.min_w = 500;
        m.min_h = 400;
        m.singleton = true;
        m.start_maximized = true;
        m.start_centered = true;
        m.dock_pinned = true;
        m.dock_order = 3;
        m.autostart = true;  // opens by default on startup
        registry.register_app(m, []() {
            return std::make_unique<FinanceApp>();
        });
    }
}
