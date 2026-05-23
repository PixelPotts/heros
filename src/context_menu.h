#pragma once
#include "draw.h"
#include <string>
#include <vector>
#include <functional>

// ── Menu Item ───────────────────────────────────────────────────

struct MenuItem {
    std::string label;
    std::string shortcut_hint;       // e.g. "Ctrl+W"
    Icon icon = Icon::Box;
    bool enabled = true;
    bool separator_after = false;
    std::function<void()> action;
};

// ── Context Menu ────────────────────────────────────────────────

class ContextMenu {
public:
    void open(int x, int y, std::vector<MenuItem> items);
    void close();
    bool is_open() const { return open_; }

    // Returns true if click was inside menu (and handled)
    bool handle_click(int mx, int my);

    // Render the menu
    void render(SDL_Renderer* r, const struct Fonts* fonts);

    // Hover update
    void on_mouse_move(int mx, int my);

private:
    bool open_ = false;
    int x_ = 0, y_ = 0;
    int w_ = 0, h_ = 0;
    int hover_index_ = -1;
    std::vector<MenuItem> items_;

    static const int ITEM_H = 28;
    static const int MENU_PAD = 6;
    static const int MENU_MIN_W = 180;
    static const int SEP_H = 8;
};
