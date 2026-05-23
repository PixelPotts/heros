#include "animation.h"
#include <algorithm>

// ── Start animations ────────────────────────────────────────────

void AnimationManager::animate_open(int window_id, SDL_Rect target) {
    WindowAnim a{};
    a.window_id = window_id;
    a.type = AnimType::WindowOpen;
    a.start_time = SDL_GetTicks();
    a.duration_ms = 200;
    // Scale up from center
    int cx = target.x + target.w / 2;
    int cy = target.y + target.h / 2;
    int sw = target.w * 85 / 100;
    int sh = target.h * 85 / 100;
    a.from_rect = {cx - sw / 2, cy - sh / 2, sw, sh};
    a.to_rect = target;
    a.from_alpha = 0.0f;
    a.to_alpha = 1.0f;
    a.from_scale = 0.85f;
    a.to_scale = 1.0f;
    anims_.push_back(a);
}

void AnimationManager::animate_close(int window_id, SDL_Rect current) {
    WindowAnim a{};
    a.window_id = window_id;
    a.type = AnimType::WindowClose;
    a.start_time = SDL_GetTicks();
    a.duration_ms = 150;
    a.from_rect = current;
    // Shrink to center
    int cx = current.x + current.w / 2;
    int cy = current.y + current.h / 2;
    int sw = current.w * 90 / 100;
    int sh = current.h * 90 / 100;
    a.to_rect = {cx - sw / 2, cy - sh / 2, sw, sh};
    a.from_alpha = 1.0f;
    a.to_alpha = 0.0f;
    a.from_scale = 1.0f;
    a.to_scale = 0.9f;
    anims_.push_back(a);
}

void AnimationManager::animate_minimize(int window_id, SDL_Rect current, SDL_Rect dock_target) {
    WindowAnim a{};
    a.window_id = window_id;
    a.type = AnimType::WindowMinimize;
    a.start_time = SDL_GetTicks();
    a.duration_ms = 250;
    a.from_rect = current;
    a.to_rect = dock_target;
    a.from_alpha = 1.0f;
    a.to_alpha = 0.0f;
    a.from_scale = 1.0f;
    a.to_scale = 0.1f;
    anims_.push_back(a);
}

void AnimationManager::animate_restore(int window_id, SDL_Rect current, SDL_Rect target) {
    WindowAnim a{};
    a.window_id = window_id;
    a.type = AnimType::WindowRestore;
    a.start_time = SDL_GetTicks();
    a.duration_ms = 200;
    a.from_rect = current;
    a.to_rect = target;
    a.from_alpha = 0.0f;
    a.to_alpha = 1.0f;
    a.from_scale = 0.1f;
    a.to_scale = 1.0f;
    anims_.push_back(a);
}

// ── Tick ────────────────────────────────────────────────────────

void AnimationManager::tick(uint32_t now) {
    for (auto& a : anims_) {
        uint32_t elapsed = now - a.start_time;
        if (elapsed >= a.duration_ms) {
            a.finished = true;
        }
    }
}

// ── Queries ─────────────────────────────────────────────────────

bool AnimationManager::is_animating(int window_id) const {
    for (auto& a : anims_) {
        if (a.window_id == window_id && !a.finished) return true;
    }
    return false;
}

bool AnimationManager::get_animated_state(int window_id, SDL_Rect& rect, float& alpha, float& scale) const {
    for (auto& a : anims_) {
        if (a.window_id == window_id && !a.finished) {
            uint32_t elapsed = SDL_GetTicks() - a.start_time;
            float t = std::min(1.0f, (float)elapsed / a.duration_ms);

            // Choose easing based on type
            float et;
            switch (a.type) {
            case AnimType::WindowOpen:
                et = ease::out_cubic(t);
                break;
            case AnimType::WindowClose:
                et = ease::in_quad(t);
                break;
            case AnimType::WindowMinimize:
                et = ease::in_out_cubic(t);
                break;
            case AnimType::WindowRestore:
                et = ease::out_back(t);
                break;
            default:
                et = ease::out_quad(t);
                break;
            }

            rect = interpolate_rect(a.from_rect, a.to_rect, et);
            alpha = interpolate(a.from_alpha, a.to_alpha, et);
            scale = interpolate(a.from_scale, a.to_scale, et);
            return true;
        }
    }
    return false;
}

std::vector<int> AnimationManager::pop_completed_closes() {
    std::vector<int> result;
    for (auto it = anims_.begin(); it != anims_.end(); ) {
        if (it->finished) {
            if (it->type == AnimType::WindowClose) {
                result.push_back(it->window_id);
            }
            it = anims_.erase(it);
        } else {
            ++it;
        }
    }
    return result;
}
