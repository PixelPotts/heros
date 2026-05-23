#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <functional>

// ── Key Binding ─────────────────────────────────────────────────

struct KeyBinding {
    std::string action;          // e.g. "app.close", "app.minimize"
    SDL_Keycode key;
    uint16_t mod;                // KMOD_CTRL, KMOD_ALT, etc.
    std::function<void()> callback;
};

// ── Shortcut Manager ────────────────────────────────────────────

class ShortcutManager {
public:
    void bind(const std::string& action, SDL_Keycode key, uint16_t mod,
              std::function<void()> callback);

    void unbind(const std::string& action);

    // Process a key event; returns true if a shortcut was triggered
    bool handle_key(SDL_Keycode key, uint16_t mod);

    const std::vector<KeyBinding>& bindings() const { return bindings_; }

private:
    std::vector<KeyBinding> bindings_;
};
