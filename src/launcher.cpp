#include "launcher.h"
#include "app_registry.h"
#include <algorithm>
#include <cctype>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};

static const int LAUNCHER_W = 500;
static const int LAUNCHER_H_BASE = 56; // search bar only
static const int RESULT_H = 44;
static const int MAX_RESULTS = 8;

// ── Open / Close ────────────────────────────────────────────────

void AppLauncher::open() {
    open_ = true;
    query_.clear();
    results_.clear();
    selected_ = 0;
    anim_alpha_ = 0;
}

void AppLauncher::close() {
    open_ = false;
    query_.clear();
    results_.clear();
}

// ── Fuzzy match ─────────────────────────────────────────────────

bool AppLauncher::fuzzy_match(const std::string& text, const std::string& query) const {
    if (query.empty()) return true;
    std::string lower_text = text;
    std::string lower_query = query;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    size_t qi = 0;
    for (size_t ti = 0; ti < lower_text.size() && qi < lower_query.size(); ti++) {
        if (lower_text[ti] == lower_query[qi]) qi++;
    }
    return qi == lower_query.size();
}

// ── Update search results ───────────────────────────────────────

void AppLauncher::update_results(AppRegistry& registry) {
    results_.clear();

    // Search installed apps
    for (auto* m : registry.list_apps()) {
        if (fuzzy_match(m->name, query_) || fuzzy_match(m->app_id, query_)) {
            LaunchResult r;
            r.label = m->name;
            r.subtitle = m->app_id;
            r.icon = m->icon;
            r.app_id = m->app_id;
            r.type = LaunchResult::App;
            results_.push_back(r);
        }
    }

    // Add built-in actions
    struct {
        const char* label;
        const char* subtitle;
        Icon icon;
        const char* app_id;
    } actions[] = {
        {"Lock Screen",   "Lock the session",   Icon::Lock,  "__lock"},
        {"Settings",      "System settings",    Icon::Gear,  "com.heros.settings"},
        {"Task Manager",  "Monitor processes",  Icon::Grid,  "com.heros.taskmanager"},
    };

    for (auto& a : actions) {
        if (fuzzy_match(a.label, query_)) {
            // Don't add if already in results as an app
            bool dup = false;
            for (auto& r : results_) {
                if (r.app_id == a.app_id) { dup = true; break; }
            }
            if (!dup) {
                results_.push_back({a.label, a.subtitle, a.icon, a.app_id, LaunchResult::Action});
            }
        }
    }

    // Sort: exact prefix matches first
    std::string lq = query_;
    std::transform(lq.begin(), lq.end(), lq.begin(), ::tolower);
    std::sort(results_.begin(), results_.end(), [&](const LaunchResult& a, const LaunchResult& b) {
        std::string la = a.label, lb = b.label;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        bool a_prefix = la.find(lq) == 0;
        bool b_prefix = lb.find(lq) == 0;
        if (a_prefix != b_prefix) return a_prefix;
        return la < lb;
    });

    // Limit results
    if (results_.size() > MAX_RESULTS) results_.resize(MAX_RESULTS);

    // Clamp selection
    if (selected_ >= (int)results_.size()) selected_ = std::max(0, (int)results_.size() - 1);
}

// ── Execute result ──────────────────────────────────────────────

void AppLauncher::execute_result(const LaunchResult& result, AppRegistry& registry,
                                 WindowManager& wm, int screen_w, int screen_h) {
    if (result.app_id == "__lock") {
        // Lock handled by caller via checking returned app_id
        close();
        return;
    }
    registry.launch(result.app_id, wm, screen_w, screen_h);
    close();
}

// ── Event handling ──────────────────────────────────────────────

bool AppLauncher::handle_event(const SDL_Event& event, AppRegistry& registry,
                               WindowManager& wm, int screen_w, int screen_h) {
    if (!open_) return false;

    if (event.type == SDL_TEXTINPUT) {
        query_ += event.text.text;
        update_results(registry);
        return true;
    }

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            close();
            return true;
        case SDLK_BACKSPACE:
            if (!query_.empty()) {
                query_.pop_back();
                update_results(registry);
            }
            return true;
        case SDLK_DOWN:
            if (selected_ < (int)results_.size() - 1) selected_++;
            return true;
        case SDLK_UP:
            if (selected_ > 0) selected_--;
            return true;
        case SDLK_RETURN:
            if (!results_.empty() && selected_ < (int)results_.size()) {
                execute_result(results_[selected_], registry, wm, screen_w, screen_h);
            }
            return true;
        case SDLK_TAB:
            // Tab cycles through results
            selected_ = (selected_ + 1) % std::max(1, (int)results_.size());
            return true;
        default:
            return true; // consume all keys when open
        }
    }

    // Consume mouse clicks (close on click outside)
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mx = event.button.x, my = event.button.y;
        int lx = (screen_w - LAUNCHER_W) / 2;
        int ly = screen_h / 4;
        int lh = LAUNCHER_H_BASE + (int)results_.size() * RESULT_H;
        if (mx < lx || mx > lx + LAUNCHER_W || my < ly || my > ly + lh) {
            close();
        } else {
            // Check if clicking a result
            int ry = ly + LAUNCHER_H_BASE;
            for (int i = 0; i < (int)results_.size(); i++) {
                if (my >= ry && my < ry + RESULT_H) {
                    selected_ = i;
                    execute_result(results_[i], registry, wm, screen_w, screen_h);
                    break;
                }
                ry += RESULT_H;
            }
        }
        return true;
    }

    return true; // consume all events when open
}

// ── Rendering ───────────────────────────────────────────────────

void AppLauncher::render(SDL_Renderer* r, FrostRenderer* frost, const Fonts* fonts,
                         int screen_w, int screen_h) {
    if (!open_) return;

    // Animate alpha
    if (anim_alpha_ < 1.0f) anim_alpha_ = std::min(1.0f, anim_alpha_ + 0.15f);

    Uint8 alpha = (Uint8)(anim_alpha_ * 255);

    // Darken background
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 15, (Uint8)(alpha * 120 / 255));
    SDL_Rect full = {0, 0, screen_w, screen_h};
    SDL_RenderFillRect(r, &full);

    int lx = (screen_w - LAUNCHER_W) / 2;
    int ly = screen_h / 4;
    int result_count = (int)results_.size();
    int lh = LAUNCHER_H_BASE + result_count * RESULT_H;

    // Main panel
    SDL_Rect panel = {lx, ly, LAUNCHER_W, lh};
    if (frost) frost->render_panel(r, panel, {10, 14, 25, (Uint8)(alpha * 200 / 255)});
    draw::rounded_rect(r, panel, 12, {180, 195, 220, (Uint8)(alpha * 40 / 255)});

    // Search icon
    draw::icon(r, Icon::Compass, lx + 24, ly + LAUNCHER_H_BASE / 2, 18,
               {200, 210, 240, alpha});

    // Search input
    std::string display = query_;
    if (query_.empty()) {
        draw::text(r, fonts->title, "Search apps, files, settings...",
                   lx + 46, ly + 16, {120, 130, 150, (Uint8)(alpha * 150 / 255)});
    } else {
        draw::text(r, fonts->title, display.c_str(), lx + 46, ly + 16,
                   {230, 230, 240, alpha});
    }

    // Cursor
    if (!query_.empty()) {
        auto sz = draw::text_size(fonts->title, display.c_str());
        int cx = lx + 46 + sz.x + 2;
        SDL_SetRenderDrawColor(r, 100, 150, 255, alpha);
        SDL_RenderDrawLine(r, cx, ly + 14, cx, ly + 40);
    }

    // Separator
    if (result_count > 0) {
        draw::line(r, lx + 12, ly + LAUNCHER_H_BASE - 2,
                   lx + LAUNCHER_W - 12, ly + LAUNCHER_H_BASE - 2,
                   {180, 195, 220, (Uint8)(alpha * 30 / 255)});
    }

    // Results
    int ry = ly + LAUNCHER_H_BASE;
    for (int i = 0; i < result_count; i++) {
        auto& res = results_[i];
        SDL_Rect item = {lx + 6, ry, LAUNCHER_W - 12, RESULT_H};

        if (i == selected_) {
            draw::filled_rounded_rect(r, item, 6, {100, 150, 255, (Uint8)(alpha * 30 / 255)});
        }

        // Icon
        draw::icon(r, res.icon, lx + 28, ry + RESULT_H / 2, 16,
                   {200, 210, 240, alpha});

        // Label
        draw::text(r, fonts->body, res.label.c_str(), lx + 50, ry + 6,
                   {230, 230, 240, alpha});

        // Subtitle
        draw::text(r, fonts->small, res.subtitle.c_str(), lx + 50, ry + 24,
                   {130, 140, 160, (Uint8)(alpha * 180 / 255)});

        // Type badge on right
        const char* badge = "";
        switch (res.type) {
        case LaunchResult::App: badge = "App"; break;
        case LaunchResult::Setting: badge = "Setting"; break;
        case LaunchResult::Action: badge = "Action"; break;
        }
        draw::text_right(r, fonts->small, badge, lx + LAUNCHER_W - 20, ry + 14,
                         {120, 130, 150, (Uint8)(alpha * 140 / 255)});

        ry += RESULT_H;
    }

    // Hint at bottom
    if (result_count > 0) {
        draw::text_centered(r, fonts->small, "Enter to launch  |  Arrow keys to navigate",
                            screen_w / 2, ry + 6, {120, 130, 150, (Uint8)(alpha * 100 / 255)});
    }
}
