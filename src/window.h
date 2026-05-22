#pragma once
#include "draw.h"
#include "frost.h"
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

// ── Forward declarations ────────────────────────────────────────

struct RenderCtx;

// ── Hit zones for mouse interaction ─────────────────────────────

enum class HitZone {
    None,
    TitleBar,
    CloseButton,
    MinimizeButton,
    MaximizeButton,
    Content,
    ResizeN, ResizeS, ResizeE, ResizeW,
    ResizeNE, ResizeNW, ResizeSE, ResizeSW
};

// ── Window capability flags ─────────────────────────────────────

enum WindowFlags : uint32_t {
    WF_Draggable   = 1 << 0,
    WF_Resizable   = 1 << 1,
    WF_Closable    = 1 << 2,
    WF_Minimizable = 1 << 3,
    WF_Maximizable = 1 << 4,
    WF_Default     = WF_Draggable | WF_Resizable | WF_Closable | WF_Minimizable | WF_Maximizable
};

// ── Abstract app content ────────────────────────────────────────

class AppContent {
public:
    virtual ~AppContent() = default;
    virtual void render(const RenderCtx& ctx, SDL_Rect content_rect) = 0;
    virtual void on_mouse_down(int local_x, int local_y) { (void)local_x; (void)local_y; }
    virtual void on_mouse_up(int local_x, int local_y) { (void)local_x; (void)local_y; }
    virtual void on_mouse_move(int local_x, int local_y) { (void)local_x; (void)local_y; }
    virtual void on_key_down(SDL_Keycode key) { (void)key; }
    virtual void on_key_up(SDL_Keycode key) { (void)key; }
    virtual void on_text_input(const char* text) { (void)text; }
};

// ── Window structure ────────────────────────────────────────────

static const int TITLEBAR_H = 32;
static const int RESIZE_EDGE = 6;
static const int BTN_SIZE = 10;
static const int BTN_SPACING = 20;
static const int BTN_MARGIN = 16;

struct Window {
    int id = 0;
    std::string title;
    Icon icon = Icon::Journal;
    SDL_Rect rect = {0, 0, 500, 380};
    SDL_Rect restore_rect = {0, 0, 0, 0};
    int min_w = 200, min_h = 150;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    bool visible = true;
    int z_order = 0;
    uint32_t flags = WF_Default;
    std::unique_ptr<AppContent> content;

    SDL_Rect title_bar_rect() const {
        return {rect.x, rect.y, rect.w, TITLEBAR_H};
    }

    SDL_Rect content_rect() const {
        return {rect.x, rect.y + TITLEBAR_H, rect.w, rect.h - TITLEBAR_H};
    }

    SDL_Rect close_btn_rect() const {
        int cx = rect.x + rect.w - BTN_MARGIN;
        int cy = rect.y + TITLEBAR_H / 2;
        return {cx - BTN_SIZE / 2, cy - BTN_SIZE / 2, BTN_SIZE, BTN_SIZE};
    }

    SDL_Rect maximize_btn_rect() const {
        int cx = rect.x + rect.w - BTN_MARGIN - BTN_SPACING;
        int cy = rect.y + TITLEBAR_H / 2;
        return {cx - BTN_SIZE / 2, cy - BTN_SIZE / 2, BTN_SIZE, BTN_SIZE};
    }

    SDL_Rect minimize_btn_rect() const {
        int cx = rect.x + rect.w - BTN_MARGIN - BTN_SPACING * 2;
        int cy = rect.y + TITLEBAR_H / 2;
        return {cx - BTN_SIZE / 2, cy - BTN_SIZE / 2, BTN_SIZE, BTN_SIZE};
    }

    HitZone hit_test(int mx, int my) const;
};

// ── Drag mode ───────────────────────────────────────────────────

enum class DragMode {
    None, Move,
    ResizeN, ResizeS, ResizeE, ResizeW,
    ResizeNE, ResizeNW, ResizeSE, ResizeSW
};

// ── Window Manager ──────────────────────────────────────────────

class WindowManager {
public:
    // Lifecycle
    int open_window(const std::string& title, Icon icon, SDL_Rect rect,
                    uint32_t flags, std::unique_ptr<AppContent> content);
    void close_window(int id);
    Window* find_window(int id);
    const Window* find_window(int id) const;

    // State
    Window* focused_window();
    const std::vector<Window>& windows() const { return windows_; }
    const std::vector<int>& minimized_ids() const { return minimized_ids_; }
    bool is_minimized(int id) const;

    // Event routing
    bool handle_event(const SDL_Event& event);

    // Rendering
    void render(const RenderCtx& ctx);

    // Window operations
    void bring_to_front(int id);
    void set_focus(int id);
    void minimize(int id);
    void maximize(int id, int screen_w, int screen_h);
    void restore(int id);
    void toggle_maximize(int id, int screen_w, int screen_h);
    void restore_from_dock(int id, int screen_w, int screen_h);

private:
    // Event handlers
    void on_mouse_down(int mx, int my, int screen_w, int screen_h);
    void on_mouse_move(int mx, int my, int screen_w, int screen_h);
    void on_mouse_up(int mx, int my);
    void on_key_event(const SDL_Event& event);

    // Helpers
    Window* window_at(int mx, int my);
    void focus_next_topmost();
    std::vector<Window*> z_sorted();

    std::vector<Window> windows_;
    std::vector<int> minimized_ids_;
    int next_id_ = 1;
    int next_z_ = 1;

    // Drag state
    DragMode drag_mode_ = DragMode::None;
    int drag_win_id_ = 0;
    int drag_offset_x_ = 0, drag_offset_y_ = 0;
    SDL_Rect drag_start_rect_ = {0, 0, 0, 0};

    // Hover state
    int hover_win_id_ = 0;
    HitZone hover_zone_ = HitZone::None;

    // Screen size cache (updated each event)
    int screen_w_ = 1280, screen_h_ = 720;
};
