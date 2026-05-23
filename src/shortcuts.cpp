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

bool ShortcutManager::handle_key(SDL_Keycode key, uint16_t mod) {
    // Normalize modifier: only care about ctrl, alt, shift, gui
    uint16_t mask = KMOD_CTRL | KMOD_ALT | KMOD_SHIFT | KMOD_GUI;
    uint16_t pressed = mod & mask;

    for (auto& b : bindings_) {
        if (b.key == key && (b.mod & mask) == pressed) {
            if (b.callback) b.callback();
            return true;
        }
    }
    return false;
}
