#include "shortcuts.h"
#include <algorithm>

void ShortcutManager::bind(const std::string& action, SDL_Keycode key, uint16_t mod,
                            std::function<void()> callback) {
    // Replace existing binding for this action
    unbind(action);
    bindings_.push_back({action, key, mod, std::move(callback)});
}

void ShortcutManager::unbind(const std::string& action) {
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [&](const KeyBinding& b) { return b.action == action; }),
        bindings_.end()
    );
}

// Collapse left/right modifier variants into combined flags
static uint16_t normalize_mod(uint16_t mod) {
    uint16_t r = 0;
    if (mod & KMOD_CTRL)  r |= KMOD_CTRL;
    if (mod & KMOD_ALT)   r |= KMOD_ALT;
    if (mod & KMOD_SHIFT) r |= KMOD_SHIFT;
    if (mod & KMOD_GUI)   r |= KMOD_GUI;
    return r;
}

bool ShortcutManager::handle_key(SDL_Keycode key, uint16_t mod) {
    uint16_t pressed = normalize_mod(mod);

    for (auto& b : bindings_) {
        if (b.key == key && normalize_mod(b.mod) == pressed) {
            if (b.callback) b.callback();
            return true;
        }
    }
    return false;
}
