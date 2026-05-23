#include "workspaces.h"
#include "window.h"
#include <algorithm>
#include <cstdio>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};

// ── Init ────────────────────────────────────────────────────────

void WorkspaceManager::init(int count) {
    workspaces_.clear();
    for (int i = 0; i < count; i++) {
        Workspace ws;
        ws.id = i;
        ws.name = "Desktop " + std::to_string(i + 1);
        workspaces_.push_back(ws);
    }
    current_ = 0;
}

// ── Switch workspace ────────────────────────────────────────────

void WorkspaceManager::switch_to(int idx, WindowManager& wm) {
    if (idx < 0 || idx >= (int)workspaces_.size() || idx == current_) return;
    current_ = idx;
    apply_visibility(wm);
    fprintf(stderr, "Workspace: switched to %s\n", workspaces_[idx].name.c_str());
}

void WorkspaceManager::switch_next(WindowManager& wm) {
    int next = (current_ + 1) % (int)workspaces_.size();
    switch_to(next, wm);
}

void WorkspaceManager::switch_prev(WindowManager& wm) {
    int prev = (current_ - 1 + (int)workspaces_.size()) % (int)workspaces_.size();
    switch_to(prev, wm);
}

// ── Window management ───────────────────────────────────────────

void WorkspaceManager::assign_window(int window_id) {
    workspaces_[current_].window_ids.insert(window_id);
}

void WorkspaceManager::move_window(int window_id, int workspace_idx, WindowManager& wm) {
    if (workspace_idx < 0 || workspace_idx >= (int)workspaces_.size()) return;

    // Remove from all workspaces
    for (auto& ws : workspaces_) {
        ws.window_ids.erase(window_id);
    }
    // Add to target
    workspaces_[workspace_idx].window_ids.insert(window_id);
    apply_visibility(wm);
}

void WorkspaceManager::remove_window(int window_id) {
    for (auto& ws : workspaces_) {
        ws.window_ids.erase(window_id);
    }
}

bool WorkspaceManager::is_on_current(int window_id) const {
    return workspaces_[current_].window_ids.count(window_id) > 0;
}

// ── Apply visibility based on current workspace ─────────────────

void WorkspaceManager::apply_visibility(WindowManager& wm) {
    const auto& current_ws = workspaces_[current_];

    for (auto& win : const_cast<std::vector<Window>&>(wm.windows())) {
        if (win.minimized) continue; // keep minimized state
        bool on_current = current_ws.window_ids.count(win.id) > 0;
        win.visible = on_current;
        if (!on_current && win.active) {
            win.active = false;
        }
    }

    // Focus topmost visible window
    Window* top = nullptr;
    for (auto& win : const_cast<std::vector<Window>&>(wm.windows())) {
        if (win.visible && !win.minimized) {
            if (!top || win.z_order > top->z_order) top = &win;
        }
    }
    if (top) {
        wm.set_focus(top->id);
    }
}

// ── Render indicator ────────────────────────────────────────────

void WorkspaceManager::render_indicator(SDL_Renderer* r, const Fonts* fonts, int cx, int y) {
    int dot_spacing = 12;
    int total_w = (int)workspaces_.size() * dot_spacing;
    int start_x = cx - total_w / 2;

    for (int i = 0; i < (int)workspaces_.size(); i++) {
        int dx = start_x + i * dot_spacing;
        if (i == current_) {
            draw::filled_circle(r, dx, y, 3, ACCENT);
        } else {
            draw::filled_circle(r, dx, y, 2, DIM);
        }
    }
    (void)fonts;
}

// ── Render switcher ─────────────────────────────────────────────

void WorkspaceManager::render_switcher(SDL_Renderer* r, const Fonts* fonts,
                                        const WindowManager& wm, int screen_w, int screen_h) {
    if (!switcher_open_) return;

    // Dark overlay
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 15, 160);
    SDL_Rect full = {0, 0, screen_w, screen_h};
    SDL_RenderFillRect(r, &full);

    int ws_count = (int)workspaces_.size();
    int thumb_w = 200, thumb_h = 120;
    int gap = 24;
    int total_w = ws_count * thumb_w + (ws_count - 1) * gap;
    int start_x = (screen_w - total_w) / 2;
    int start_y = screen_h / 2 - thumb_h / 2 - 20;

    draw::text_centered(r, fonts->title, "Workspaces", screen_w / 2, start_y - 40, WHITE);

    for (int i = 0; i < ws_count; i++) {
        int tx = start_x + i * (thumb_w + gap);
        int ty = start_y;

        SDL_Rect thumb = {tx, ty, thumb_w, thumb_h};

        // Highlight current workspace
        if (i == current_) {
            draw::filled_rounded_rect(r, {tx - 3, ty - 3, thumb_w + 6, thumb_h + 6}, 10, {100, 150, 255, 40});
            draw::rounded_rect(r, {tx - 3, ty - 3, thumb_w + 6, thumb_h + 6}, 10, ACCENT);
        }

        // Thumbnail background
        draw::filled_rounded_rect(r, thumb, 8, {20, 25, 40, 200});
        draw::rounded_rect(r, thumb, 8, {180, 195, 220, 40});

        // Mini window representations
        for (auto& win : wm.windows()) {
            if (workspaces_[i].window_ids.count(win.id) && !win.minimized) {
                // Scale window rect to thumbnail
                float sx = (float)thumb_w / screen_w;
                float sy = (float)thumb_h / screen_h;
                SDL_Rect mini = {
                    tx + (int)(win.rect.x * sx),
                    ty + (int)(win.rect.y * sy),
                    std::max(8, (int)(win.rect.w * sx)),
                    std::max(6, (int)(win.rect.h * sy))
                };
                draw::filled_rounded_rect(r, mini, 2, {40, 50, 70, 180});
                draw::rounded_rect(r, mini, 2, {140, 155, 180, 100});
            }
        }

        // Label
        draw::text_centered(r, fonts->body, workspaces_[i].name.c_str(),
                            tx + thumb_w / 2, ty + thumb_h + 8, WHITE);

        // Window count
        int wcount = (int)workspaces_[i].window_ids.size();
        if (wcount > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", wcount);
            draw::text_centered(r, fonts->small, buf, tx + thumb_w / 2, ty + thumb_h + 26, DIM);
        }
    }

    draw::text_centered(r, fonts->small, "Click to switch  |  Ctrl+Alt+Arrow to navigate",
                        screen_w / 2, start_y + thumb_h + 50, {120, 130, 150, 150});
}

// ── Handle switcher click ───────────────────────────────────────

bool WorkspaceManager::handle_switcher_click(int mx, int my, WindowManager& wm,
                                              int screen_w, int screen_h) {
    if (!switcher_open_) return false;

    int ws_count = (int)workspaces_.size();
    int thumb_w = 200, thumb_h = 120;
    int gap = 24;
    int total_w = ws_count * thumb_w + (ws_count - 1) * gap;
    int start_x = (screen_w - total_w) / 2;
    int start_y = screen_h / 2 - thumb_h / 2 - 20;

    for (int i = 0; i < ws_count; i++) {
        int tx = start_x + i * (thumb_w + gap);
        if (mx >= tx && mx < tx + thumb_w && my >= start_y && my < start_y + thumb_h) {
            switch_to(i, wm);
            switcher_open_ = false;
            return true;
        }
    }

    // Click outside closes
    switcher_open_ = false;
    return true;
}
