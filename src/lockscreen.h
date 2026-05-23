#pragma once
#include "draw.h"
#include "frost.h"
#include <string>
#include <cstdint>

class AudioManager;

// ── Lock Screen / Login Screen ──────────────────────────────────

enum class ScreenState {
    Login,      // initial boot — needs username + password
    Locked,     // session locked — needs password only
    Desktop     // unlocked, normal operation
};

class LockScreen {
public:
    void init(const std::string& settings_path);

    // State
    ScreenState state() const { return state_; }
    bool is_locked() const { return state_ != ScreenState::Desktop; }

    // Actions
    void lock();
    void unlock();
    void set_state(ScreenState s) { state_ = s; }

    // User setup (first time)
    bool has_user() const { return !username_.empty(); }
    void create_user(const std::string& username, const std::string& password);
    const std::string& username() const { return username_; }

    // Event handling — returns true if event was consumed
    bool handle_event(const SDL_Event& event);

    // Rendering
    void render(SDL_Renderer* r, FrostRenderer* frost, Fonts* fonts,
                int w, int h, SDL_Texture* wallpaper);

    void set_audio(AudioManager* a) { audio_ = a; }

private:
    ScreenState state_ = ScreenState::Login;
    std::string username_;
    std::string password_hash_;
    std::string input_text_;
    std::string input_user_;
    bool show_error_ = false;
    uint32_t error_time_ = 0;
    bool entering_username_ = true; // for login: toggle user/pass fields
    float anim_phase_ = 0;

    std::string settings_path_;
    AudioManager* audio_ = nullptr;

    // Simple hash for stored password
    static std::string hash_password(const std::string& pw);
    bool verify_password(const std::string& pw) const;
    void save_credentials();
    void load_credentials();
};
