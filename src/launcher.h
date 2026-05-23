#pragma once
#include "draw.h"
#include "frost.h"
#include <string>
#include <vector>
#include <functional>

class AppRegistry;
class WindowManager;

// ── Launcher search result ──────────────────────────────────────

struct LaunchResult {
    std::string label;
    std::string subtitle;
    Icon icon;
    std::string app_id;
    enum Type { App, Setting, Action } type;
};

// ── Spotlight-style App Launcher ────────────────────────────────

class AppLauncher {
public:
    bool is_open() const { return open_; }
    void open();
    void close();
    void toggle() { if (open_) close(); else open(); }

    // Event handling — returns true if consumed
    bool handle_event(const SDL_Event& event, AppRegistry& registry,
                      WindowManager& wm, int screen_w, int screen_h);

    // Render
    void render(SDL_Renderer* r, FrostRenderer* frost, const Fonts* fonts,
                int screen_w, int screen_h);

private:
    bool open_ = false;
    std::string query_;
    std::vector<LaunchResult> results_;
    int selected_ = 0;
    float anim_alpha_ = 0;

    void update_results(AppRegistry& registry);
    bool fuzzy_match(const std::string& text, const std::string& query) const;
    void execute_result(const LaunchResult& result, AppRegistry& registry,
                       WindowManager& wm, int screen_w, int screen_h);
};
