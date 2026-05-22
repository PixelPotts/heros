#pragma once
#include "../window.h"

class JournalApp : public AppContent {
public:
    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_down(int local_x, int local_y) override;
    void on_mouse_move(int local_x, int local_y) override;

private:
    int active_nav_ = 0;
    int hover_nav_ = -1;
    int hover_tool_ = -1;

    // Layout cache (set during render, used for hit testing)
    int nav_w_ = 110;
    int nav_item_count_ = 5;
    int tool_count_ = 6;
    SDL_Rect last_content_rect_ = {0, 0, 0, 0};
};
