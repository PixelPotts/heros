#pragma once
#include "draw.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>

class WindowManager;

// ── Workspace ───────────────────────────────────────────────────

struct Workspace {
    int id;
    std::string name;
    std::unordered_set<int> window_ids; // windows on this workspace
};

// ── Workspace Manager ───────────────────────────────────────────

class WorkspaceManager {
public:
    void init(int count = 4);

    int current() const { return current_; }
    int count() const { return (int)workspaces_.size(); }
    const Workspace& workspace(int idx) const { return workspaces_[idx]; }

    // Switch workspace
    void switch_to(int idx, WindowManager& wm);
    void switch_next(WindowManager& wm);
    void switch_prev(WindowManager& wm);

    // Window management
    void assign_window(int window_id);  // assign to current workspace
    void move_window(int window_id, int workspace_idx, WindowManager& wm);
    void remove_window(int window_id);  // untrack closed window

    // Check if a window belongs to current workspace
    bool is_on_current(int window_id) const;

    // Switcher overlay
    bool switcher_open() const { return switcher_open_; }
    void open_switcher() { switcher_open_ = true; }
    void close_switcher() { switcher_open_ = false; }
    void toggle_switcher() { switcher_open_ = !switcher_open_; }

    // Render workspace indicator (in topbar or dock)
    void render_indicator(SDL_Renderer* r, const Fonts* fonts, int cx, int y);

    // Render switcher overlay
    void render_switcher(SDL_Renderer* r, const Fonts* fonts,
                         const WindowManager& wm, int screen_w, int screen_h);

    // Handle switcher click — returns true if consumed
    bool handle_switcher_click(int mx, int my, WindowManager& wm,
                               int screen_w, int screen_h);

private:
    std::vector<Workspace> workspaces_;
    int current_ = 0;
    bool switcher_open_ = false;

    void apply_visibility(WindowManager& wm);
};
