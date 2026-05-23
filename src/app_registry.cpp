#include "app_registry.h"
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <dlfcn.h>

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

    apps_[manifest.app_id] = {manifest, std::move(factory), nullptr};
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
            actx.pm = pm_;
            actx.fs = fs_;
            actx.settings = settings_;
            actx.bus = bus_;
            actx.clipboard = clipboard_;
            actx.notifications = notifications_;
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

// ── Icon name → enum parser ──────────────────────────────────────

Icon AppRegistry::parse_icon(const std::string& name) {
    if (name == "bell")       return Icon::Bell;
    if (name == "waveform")   return Icon::Waveform;
    if (name == "grid")       return Icon::Grid;
    if (name == "volume")     return Icon::Volume;
    if (name == "power")      return Icon::Power;
    if (name == "flower")     return Icon::Flower;
    if (name == "book")       return Icon::Book;
    if (name == "journal")    return Icon::Journal;
    if (name == "briefcase")  return Icon::Briefcase;
    if (name == "sliders")    return Icon::Sliders;
    if (name == "compass")    return Icon::Compass;
    if (name == "people")     return Icon::People;
    if (name == "gear")       return Icon::Gear;
    if (name == "star")       return Icon::Star;
    if (name == "sparkle")    return Icon::Sparkle;
    if (name == "moon")       return Icon::Moon;
    if (name == "target")     return Icon::Target;
    if (name == "lotus")      return Icon::Lotus;
    if (name == "mountain")   return Icon::Mountain;
    if (name == "trash")      return Icon::Trash;
    if (name == "pen")        return Icon::Pen;
    if (name == "image")      return Icon::Image;
    if (name == "pin")        return Icon::Pin;
    if (name == "check")      return Icon::Check;
    if (name == "lock")       return Icon::Lock;
    if (name == "dots")       return Icon::Dots;
    if (name == "ring")       return Icon::Ring;
    return Icon::Box;  // default fallback
}

// ── Category string → enum parser ───────────────────────────────

AppCategory AppRegistry::parse_category(const std::string& name) {
    if (name == "productivity")  return AppCategory::Productivity;
    if (name == "system")        return AppCategory::System;
    if (name == "utility")       return AppCategory::Utility;
    if (name == "creative")      return AppCategory::Creative;
    if (name == "communication") return AppCategory::Communication;
    return AppCategory::Utility;  // default fallback
}

// ── Parse manifest.conf → AppManifest ───────────────────────────

bool AppRegistry::parse_manifest(const std::string& path, AppManifest& out) const {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "app_id")           out.app_id = val;
        else if (key == "name")        out.name = val;
        else if (key == "icon")        out.icon = parse_icon(val);
        else if (key == "category")    out.category = parse_category(val);
        else if (key == "singleton")   out.singleton = (val == "true");
        else if (key == "start_maximized") out.start_maximized = (val == "true");
        else if (key == "start_centered")  out.start_centered = (val == "true");
        else if (key == "dock_pinned")     out.dock_pinned = (val == "true");
        else if (key == "autostart")       out.autostart = (val == "true");
        else if (key == "dock_order")      out.dock_order = std::stoi(val);
        else if (key == "default_w")       out.default_rect.w = std::stoi(val);
        else if (key == "default_h")       out.default_rect.h = std::stoi(val);
        else if (key == "min_w")           out.min_w = std::stoi(val);
        else if (key == "min_h")           out.min_h = std::stoi(val);
        else if (key == "max_w")           out.max_w = std::stoi(val);
        else if (key == "max_h")           out.max_h = std::stoi(val);
        else if (key == "version")         out.version = val;
        // "library" key is handled by the caller
    }

    return !out.app_id.empty() && !out.name.empty();
}

// ── Dynamic app loading ─────────────────────────────────────────

void AppRegistry::load_dynamic_apps() {
    namespace fs = std::filesystem;

    const char* home = getenv("HOME");
    if (!home) return;

    std::string apps_dir = std::string(home) + "/.heros/apps";
    if (!fs::is_directory(apps_dir)) {
        fprintf(stderr, "AppLoader: no apps directory at %s\n", apps_dir.c_str());
        return;
    }

    for (auto& bundle : fs::directory_iterator(apps_dir)) {
        if (!bundle.is_directory()) continue;

        std::string bundle_path = bundle.path().string();
        std::string manifest_path = bundle_path + "/manifest.conf";

        if (!fs::exists(manifest_path)) {
            fprintf(stderr, "AppLoader: skipping %s (no manifest.conf)\n",
                    bundle.path().filename().c_str());
            continue;
        }

        // Parse manifest
        AppManifest manifest;
        if (!parse_manifest(manifest_path, manifest)) {
            fprintf(stderr, "AppLoader: failed to parse %s\n", manifest_path.c_str());
            continue;
        }

        // Find library path from manifest
        std::string lib_name;
        {
            std::ifstream mf(manifest_path);
            std::string line;
            while (std::getline(mf, line)) {
                if (line.substr(0, 8) == "library=") {
                    lib_name = line.substr(8);
                    break;
                }
            }
        }
        if (lib_name.empty()) {
            fprintf(stderr, "AppLoader: no library= in %s\n", manifest_path.c_str());
            continue;
        }

        std::string so_path = bundle_path + "/" + lib_name;
        if (!fs::exists(so_path)) {
            fprintf(stderr, "AppLoader: library not found: %s\n", so_path.c_str());
            continue;
        }

        // dlopen the shared library
        void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "AppLoader: dlopen failed for %s: %s\n",
                    so_path.c_str(), dlerror());
            continue;
        }

        // Resolve symbols
        using InfoFn   = const HerosAppInfo* (*)();
        using CreateFn = AppContent* (*)();

        auto info_fn   = (InfoFn)dlsym(handle, "heros_app_info");
        auto create_fn = (CreateFn)dlsym(handle, "heros_create_app");

        if (!info_fn || !create_fn) {
            fprintf(stderr, "AppLoader: missing exports in %s: %s\n",
                    so_path.c_str(), dlerror());
            dlclose(handle);
            continue;
        }

        // ABI version check
        const HerosAppInfo* info = info_fn();
        if (!info || info->abi_version != HEROS_ABI_VERSION) {
            fprintf(stderr, "AppLoader: ABI mismatch in %s (got %d, want %d)\n",
                    so_path.c_str(), info ? info->abi_version : -1, HEROS_ABI_VERSION);
            dlclose(handle);
            continue;
        }

        // Skip if already registered (duplicate app_id)
        if (apps_.count(manifest.app_id)) {
            fprintf(stderr, "AppLoader: skipping duplicate app_id '%s'\n",
                    manifest.app_id.c_str());
            dlclose(handle);
            continue;
        }

        // Wrap raw factory in unique_ptr-returning lambda
        AppFactory factory = [create_fn]() -> std::unique_ptr<AppContent> {
            AppContent* raw = create_fn();
            return std::unique_ptr<AppContent>(raw);
        };

        apps_[manifest.app_id] = {manifest, std::move(factory), handle};
        fprintf(stderr, "AppLoader: loaded dynamic app '%s' (%s) from %s\n",
                manifest.name.c_str(), manifest.app_id.c_str(), so_path.c_str());
    }
}

void AppRegistry::unload_all_dynamic() {
    for (auto& [id, entry] : apps_) {
        if (entry.dl_handle) {
            dlclose(entry.dl_handle);
            fprintf(stderr, "AppLoader: unloaded '%s'\n", id.c_str());
            entry.dl_handle = nullptr;
        }
    }
}

// ── Built-in app registration (stub — apps now load dynamically) ─

void register_builtin_apps(AppRegistry& /*registry*/) {
    // All apps are now loaded as dynamic .so plugins from ~/.heros/apps/
    // This function is kept as a stub for future built-in system apps.
}
