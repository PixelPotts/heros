#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include <cmath>

// ── Easing functions ────────────────────────────────────────────

namespace ease {
    inline float linear(float t) { return t; }
    inline float in_quad(float t) { return t * t; }
    inline float out_quad(float t) { return t * (2 - t); }
    inline float in_out_quad(float t) {
        return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }
    inline float out_cubic(float t) { float t1 = t - 1; return t1 * t1 * t1 + 1; }
    inline float in_out_cubic(float t) {
        return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
    }
    inline float out_back(float t) {
        float c1 = 1.70158f;
        float c3 = c1 + 1;
        return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
    }
    inline float out_elastic(float t) {
        if (t == 0 || t == 1) return t;
        return powf(2, -10 * t) * sinf((t * 10 - 0.75f) * (2 * (float)M_PI) / 3) + 1;
    }
}

// ── Animation types ─────────────────────────────────────────────

enum class AnimType {
    WindowOpen,
    WindowClose,
    WindowMinimize,
    WindowRestore,
    WindowMaximize
};

// ── Window Animation ────────────────────────────────────────────

struct WindowAnim {
    int window_id;
    AnimType type;
    uint32_t start_time;
    uint32_t duration_ms;
    SDL_Rect from_rect;
    SDL_Rect to_rect;
    float from_alpha;
    float to_alpha;
    float from_scale;
    float to_scale;
    bool finished = false;
};

// ── Animation Manager ───────────────────────────────────────────

class AnimationManager {
public:
    // Start a new animation
    void animate_open(int window_id, SDL_Rect target);
    void animate_close(int window_id, SDL_Rect current);
    void animate_minimize(int window_id, SDL_Rect current, SDL_Rect dock_target);
    void animate_restore(int window_id, SDL_Rect current, SDL_Rect target);

    // Tick: update all animations. Returns list of windows being animated.
    void tick(uint32_t now);

    // Query: is this window currently animating?
    bool is_animating(int window_id) const;

    // Get the current animated rect/alpha for a window
    bool get_animated_state(int window_id, SDL_Rect& rect, float& alpha, float& scale) const;

    // Get window IDs of completed close animations (so WM can actually remove them)
    std::vector<int> pop_completed_closes();

    // Check if any animation is active
    bool has_active() const { return !anims_.empty(); }

private:
    std::vector<WindowAnim> anims_;

    float interpolate(float from, float to, float t) const {
        return from + (to - from) * t;
    }

    SDL_Rect interpolate_rect(const SDL_Rect& from, const SDL_Rect& to, float t) const {
        return {
            (int)interpolate((float)from.x, (float)to.x, t),
            (int)interpolate((float)from.y, (float)to.y, t),
            (int)interpolate((float)from.w, (float)to.w, t),
            (int)interpolate((float)from.h, (float)to.h, t)
        };
    }
};
