#include "context_menu.h"
#include <algorithm>

static const SDL_Color MENU_BG    = {18, 22, 45, 240};
static const SDL_Color MENU_HOVER = {100, 150, 255, 30};
static const SDL_Color WHITE      = {230, 230, 240, 255};
static const SDL_Color DIM        = {150, 160, 180, 255};
static const SDL_Color DISABLED   = {80, 85, 100, 255};
static const SDL_Color BORDER     = {180, 195, 220, 30};
static const SDL_Color SEP_COL    = {255, 255, 255, 15};

void ContextMenu::open(int x, int y, std::vector<MenuItem> items) {
    x_ = x;
    y_ = y;
    items_ = std::move(items);
    hover_index_ = -1;
    open_ = true;

    // Calculate dimensions
    w_ = MENU_MIN_W;
    h_ = MENU_PAD * 2;
    for (auto& item : items_) {
        h_ += ITEM_H;
        if (item.separator_after) h_ += SEP_H;
    }
}

void ContextMenu::close() {
    open_ = false;
    items_.clear();
    hover_index_ = -1;
}

bool ContextMenu::handle_click(int mx, int my) {
    if (!open_) return false;

    // Check if outside
    if (mx < x_ || mx >= x_ + w_ || my < y_ || my >= y_ + h_) {
        close();
        return true; // consumed the click (closed the menu)
    }

    // Find clicked item
    int iy = y_ + MENU_PAD;
    for (int i = 0; i < (int)items_.size(); i++) {
        if (my >= iy && my < iy + ITEM_H) {
            if (items_[i].enabled && items_[i].action) {
                items_[i].action();
            }
            close();
            return true;
        }
        iy += ITEM_H;
        if (items_[i].separator_after) iy += SEP_H;
    }

    close();
    return true;
}

void ContextMenu::on_mouse_move(int mx, int my) {
    if (!open_) return;

    hover_index_ = -1;
    if (mx < x_ || mx >= x_ + w_ || my < y_ || my >= y_ + h_) return;

    int iy = y_ + MENU_PAD;
    for (int i = 0; i < (int)items_.size(); i++) {
        if (my >= iy && my < iy + ITEM_H) {
            hover_index_ = i;
            return;
        }
        iy += ITEM_H;
        if (items_[i].separator_after) iy += SEP_H;
    }
}

void ContextMenu::render(SDL_Renderer* r, const Fonts* fonts) {
    if (!open_) return;

    // Background
    draw::filled_rounded_rect(r, {x_, y_, w_, h_}, 8, MENU_BG);
    draw::rounded_rect(r, {x_, y_, w_, h_}, 8, BORDER);

    int iy = y_ + MENU_PAD;
    for (int i = 0; i < (int)items_.size(); i++) {
        auto& item = items_[i];

        // Hover highlight
        if (i == hover_index_ && item.enabled) {
            draw::filled_rounded_rect(r, {x_ + 4, iy, w_ - 8, ITEM_H}, 4, MENU_HOVER);
        }

        // Icon
        SDL_Color ic = item.enabled ? DIM : DISABLED;
        draw::icon(r, item.icon, x_ + 20, iy + ITEM_H / 2, 12, ic);

        // Label
        SDL_Color lc = item.enabled ? WHITE : DISABLED;
        draw::text(r, fonts->body, item.label.c_str(), x_ + 34, iy + 6, lc);

        // Shortcut hint
        if (!item.shortcut_hint.empty()) {
            draw::text_right(r, fonts->small, item.shortcut_hint.c_str(),
                             x_ + w_ - 10, iy + 8, DIM);
        }

        iy += ITEM_H;

        // Separator
        if (item.separator_after) {
            draw::line(r, x_ + 10, iy + SEP_H / 2, x_ + w_ - 10, iy + SEP_H / 2, SEP_COL);
            iy += SEP_H;
        }
    }
}
