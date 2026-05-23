#include "lockscreen.h"
#include "audio.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <functional>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};
static const SDL_Color ERROR_COL = {255, 100, 100, 255};

// ── Simple password hash (djb2 + salt) ──────────────────────────

std::string LockScreen::hash_password(const std::string& pw) {
    // Simple hash for demo — NOT cryptographically secure
    std::string salted = "heros_salt_" + pw + "_pepper";
    std::size_t h = std::hash<std::string>{}(salted);
    char buf[32];
    snprintf(buf, sizeof(buf), "%016zx", h);
    return std::string(buf);
}

bool LockScreen::verify_password(const std::string& pw) const {
    return hash_password(pw) == password_hash_;
}

// ── Persistence ─────────────────────────────────────────────────

void LockScreen::init(const std::string& settings_path) {
    settings_path_ = settings_path;
    load_credentials();

    if (has_user()) {
        state_ = ScreenState::Locked;
    } else {
        state_ = ScreenState::Login;
        entering_username_ = true;
    }
}

void LockScreen::save_credentials() {
    std::ofstream f(settings_path_);
    if (f.is_open()) {
        f << "username=" << username_ << "\n";
        f << "password_hash=" << password_hash_ << "\n";
    }
}

void LockScreen::load_credentials() {
    std::ifstream f(settings_path_);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "username") username_ = val;
        else if (key == "password_hash") password_hash_ = val;
    }
}

void LockScreen::create_user(const std::string& username, const std::string& password) {
    username_ = username;
    password_hash_ = hash_password(password);
    save_credentials();
}

void LockScreen::lock() {
    state_ = ScreenState::Locked;
    input_text_.clear();
    show_error_ = false;
    if (audio_) audio_->play(SystemSound::Lock);
}

void LockScreen::unlock() {
    state_ = ScreenState::Desktop;
    input_text_.clear();
    show_error_ = false;
    if (audio_) audio_->play(SystemSound::Unlock);
}

// ── Event handling ──────────────────────────────────────────────

bool LockScreen::handle_event(const SDL_Event& event) {
    if (state_ == ScreenState::Desktop) return false;

    if (event.type == SDL_TEXTINPUT) {
        input_text_ += event.text.text;
        show_error_ = false;
        return true;
    }

    if (event.type == SDL_KEYDOWN) {
        SDL_Keycode key = event.key.keysym.sym;

        if (key == SDLK_BACKSPACE && !input_text_.empty()) {
            input_text_.pop_back();
            show_error_ = false;
            return true;
        }

        if (key == SDLK_RETURN) {
            if (state_ == ScreenState::Login && !has_user()) {
                // Creating new user
                if (entering_username_) {
                    if (!input_text_.empty()) {
                        input_user_ = input_text_;
                        input_text_.clear();
                        entering_username_ = false;
                    }
                } else {
                    if (!input_text_.empty()) {
                        create_user(input_user_, input_text_);
                        unlock();
                    }
                }
            } else {
                // Verifying password (locked or login with existing user)
                if (verify_password(input_text_)) {
                    unlock();
                } else {
                    show_error_ = true;
                    error_time_ = SDL_GetTicks();
                    input_text_.clear();
                    if (audio_) audio_->play(SystemSound::Error);
                }
            }
            return true;
        }

        if (key == SDLK_ESCAPE) {
            input_text_.clear();
            show_error_ = false;
            return true;
        }

        return true; // consume all keys when locked
    }

    // Consume mouse events too
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
        event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEWHEEL)
        return true;

    return false;
}

// ── Rendering ───────────────────────────────────────────────────

void LockScreen::render(SDL_Renderer* r, FrostRenderer* frost, Fonts* fonts,
                        int w, int h, SDL_Texture* wallpaper) {
    if (state_ == ScreenState::Desktop) return;

    anim_phase_ += 0.02f;

    // Draw wallpaper background
    if (wallpaper) {
        int tw, th;
        SDL_QueryTexture(wallpaper, nullptr, nullptr, &tw, &th);
        float scale = std::max((float)w / tw, (float)h / th);
        int dw = (int)(tw * scale), dh = (int)(th * scale);
        SDL_Rect dst = {(w - dw) / 2, (h - dh) / 2, dw, dh};
        SDL_RenderCopy(r, wallpaper, nullptr, &dst);
    }

    // Dark overlay
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 15, 180);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);

    int cx = w / 2;
    int cy = h / 2 - 40;

    // Animated glow ring
    float pulse = 0.5f + 0.5f * sinf(anim_phase_);
    Uint8 glow_a = (Uint8)(40 + 30 * pulse);
    draw::circle(r, cx, cy - 60, 40, {100, 150, 255, glow_a});
    draw::circle(r, cx, cy - 60, 42, {100, 150, 255, (Uint8)(glow_a / 2)});
    draw::glow(r, cx, cy - 60, 20, {100, 150, 255, (Uint8)(60 * pulse)});

    // Lock/user icon
    if (state_ == ScreenState::Locked) {
        draw::icon(r, Icon::Lock, cx, cy - 60, 28, {200, 210, 240, 220});
    } else {
        draw::icon(r, Icon::People, cx, cy - 60, 28, {200, 210, 240, 220});
    }

    // Title
    if (state_ == ScreenState::Locked) {
        draw::text_centered(r, fonts->widget, username_.c_str(), cx, cy - 10, WHITE);
        draw::text_centered(r, fonts->body, "Enter password to unlock", cx, cy + 20, DIM);
    } else if (!has_user()) {
        draw::text_centered(r, fonts->widget, "Welcome to HerOS", cx, cy - 10, WHITE);
        if (entering_username_) {
            draw::text_centered(r, fonts->body, "Choose a username", cx, cy + 20, DIM);
        } else {
            draw::text_centered(r, fonts->body, "Set a password", cx, cy + 20, DIM);
        }
    } else {
        draw::text_centered(r, fonts->widget, username_.c_str(), cx, cy - 10, WHITE);
        draw::text_centered(r, fonts->body, "Enter password", cx, cy + 20, DIM);
    }

    // Input field
    int field_w = 280, field_h = 36;
    SDL_Rect field = {cx - field_w / 2, cy + 50, field_w, field_h};

    // Frost panel for input field
    if (frost) frost->render_panel(r, field, {10, 14, 25, 140});
    draw::rounded_rect(r, field, 8, show_error_ ? ERROR_COL : SDL_Color{180, 195, 220, 60});

    // Display text (masked for password)
    bool is_password = (state_ == ScreenState::Locked) ||
                       (state_ == ScreenState::Login && !entering_username_);
    std::string display;
    if (is_password) {
        display = std::string(input_text_.size(), '*');
    } else {
        display = input_text_;
    }

    // Cursor blink
    bool show_cursor = ((int)(anim_phase_ * 8) % 2) == 0;
    if (show_cursor) display += "|";

    draw::text(r, fonts->body, display.c_str(), field.x + 12, field.y + 10, WHITE);

    // Error message
    if (show_error_) {
        uint32_t elapsed = SDL_GetTicks() - error_time_;
        if (elapsed < 3000) {
            Uint8 ea = (Uint8)(255 * (1.0f - (float)elapsed / 3000));
            draw::text_centered(r, fonts->small, "Incorrect password",
                                cx, cy + 95, {255, 100, 100, ea});
        } else {
            show_error_ = false;
        }
    }

    // Hint text at bottom
    draw::text_centered(r, fonts->small, "Press Enter to submit",
                        cx, h - 60, {150, 160, 180, 120});

    // Time display at top
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t->tm_hour, t->tm_min);
    draw::text_centered(r, fonts->large, time_str, cx, 60, WHITE);

    char date_str[64];
    strftime(date_str, sizeof(date_str), "%A, %B %d", t);
    draw::text_centered(r, fonts->body, date_str, cx, 105, DIM);
}
