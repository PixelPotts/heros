#pragma once
#include "draw.h"
#include "frost.h"
#include <string>
#include <cstdint>
#include <functional>

class AudioManager;
class LockScreen;

// ── Power actions ───────────────────────────────────────────────

enum class PowerAction {
    None,
    Shutdown,
    Restart,
    Sleep,
    Logout,
    Lock
};

// ── Power Manager ───────────────────────────────────────────────

class PowerManager {
public:
    void init(AudioManager* audio, LockScreen* lockscreen);

    // Menu state
    bool menu_open() const { return menu_open_; }
    void open_menu();
    void close_menu() { menu_open_ = false; }
    void toggle_menu() { if (menu_open_) close_menu(); else open_menu(); }

    // Idle tracking
    void on_activity() { last_activity_ = SDL_GetTicks(); }
    void tick(uint32_t now);

    // Screen dimming
    bool is_dimming() const { return dimming_; }
    float dim_alpha() const { return dim_alpha_; }

    // Event handling
    bool handle_click(int mx, int my, int screen_w, int screen_h);

    // Render power menu
    void render_menu(SDL_Renderer* r, FrostRenderer* frost, const Fonts* fonts,
                     int screen_w, int screen_h);

    // Render screen dim overlay (call last)
    void render_dim(SDL_Renderer* r, int screen_w, int screen_h);

    // Check if user requested quit
    bool should_quit() const { return should_quit_; }
    bool should_restart() const { return should_restart_; }

    // Callbacks
    void set_on_lock(std::function<void()> fn) { on_lock_ = fn; }

    // Idle timeout settings (ms)
    void set_dim_timeout(uint32_t ms) { dim_timeout_ = ms; }
    void set_lock_timeout(uint32_t ms) { lock_timeout_ = ms; }

private:
    bool menu_open_ = false;
    int hover_item_ = -1;
    bool should_quit_ = false;
    bool should_restart_ = false;
    bool sleeping_ = false;
    bool dimming_ = false;
    float dim_alpha_ = 0;
    uint32_t last_activity_ = 0;
    uint32_t dim_timeout_ = 120000;  // 2 minutes
    uint32_t lock_timeout_ = 300000; // 5 minutes

    AudioManager* audio_ = nullptr;
    LockScreen* lockscreen_ = nullptr;
    std::function<void()> on_lock_;

    void execute(PowerAction action);

    struct MenuItem {
        const char* label;
        const char* desc;
        Icon icon;
        PowerAction action;
    };
    static const MenuItem MENU_ITEMS[];
    static const int MENU_ITEM_COUNT;
};
