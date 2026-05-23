#pragma once
#include <SDL2/SDL.h>
#include <vector>

// ── Layout Utilities ────────────────────────────────────────────
// Simple layout helpers for positioning widgets within a content rect.
// Not a full constraint system — just the common patterns apps need.

namespace layout {

// ── Inset / Padding ─────────────────────────────────────────────

inline SDL_Rect inset(SDL_Rect r, int pad) {
    return {r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
}

inline SDL_Rect inset(SDL_Rect r, int h_pad, int v_pad) {
    return {r.x + h_pad, r.y + v_pad, r.w - h_pad * 2, r.h - v_pad * 2};
}

// ── Split ───────────────────────────────────────────────────────

inline void split_h(SDL_Rect r, float ratio, int gap, SDL_Rect& left, SDL_Rect& right) {
    int lw = (int)((r.w - gap) * ratio);
    left  = {r.x, r.y, lw, r.h};
    right = {r.x + lw + gap, r.y, r.w - lw - gap, r.h};
}

inline void split_v(SDL_Rect r, float ratio, int gap, SDL_Rect& top, SDL_Rect& bottom) {
    int th = (int)((r.h - gap) * ratio);
    top    = {r.x, r.y, r.w, th};
    bottom = {r.x, r.y + th + gap, r.w, r.h - th - gap};
}

// ── Column/Row distribution ─────────────────────────────────────

struct Rect { int x, y, w, h; };

inline std::vector<SDL_Rect> columns(SDL_Rect r, int count, int gap) {
    std::vector<SDL_Rect> result;
    int cw = (r.w - gap * (count - 1)) / count;
    for (int i = 0; i < count; i++) {
        result.push_back({r.x + i * (cw + gap), r.y, cw, r.h});
    }
    // Adjust last column to fill remaining space
    if (!result.empty()) {
        auto& last = result.back();
        last.w = r.x + r.w - last.x;
    }
    return result;
}

inline std::vector<SDL_Rect> rows(SDL_Rect r, int count, int gap) {
    std::vector<SDL_Rect> result;
    int rh = (r.h - gap * (count - 1)) / count;
    for (int i = 0; i < count; i++) {
        result.push_back({r.x, r.y + i * (rh + gap), r.w, rh});
    }
    if (!result.empty()) {
        auto& last = result.back();
        last.h = r.y + r.h - last.y;
    }
    return result;
}

// ── Stack Layout ────────────────────────────────────────────────
// Tracks vertical cursor for sequential layout

class VStack {
public:
    VStack(int x, int y, int w, int gap = 8)
        : x_(x), w_(w), gap_(gap), cursor_(y) {}

    // Reserve space and return rect
    SDL_Rect next(int height) {
        SDL_Rect r = {x_, cursor_, w_, height};
        cursor_ += height + gap_;
        return r;
    }

    // Skip space
    void skip(int h) { cursor_ += h; }

    // Current y position
    int y() const { return cursor_; }

    // Total height consumed
    int total_height(int start_y) const { return cursor_ - start_y; }

    int x() const { return x_; }
    int w() const { return w_; }

private:
    int x_, w_, gap_;
    int cursor_;
};

// ── Center helper ───────────────────────────────────────────────

inline SDL_Rect center_in(SDL_Rect outer, int w, int h) {
    return {outer.x + (outer.w - w) / 2, outer.y + (outer.h - h) / 2, w, h};
}

inline SDL_Rect align_right(SDL_Rect outer, int w, int h, int margin = 0) {
    return {outer.x + outer.w - w - margin, outer.y + (outer.h - h) / 2, w, h};
}

} // namespace layout
