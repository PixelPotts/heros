#pragma once
#include "../window.h"
#include <string>
#include <vector>

class TaskManagerApp : public AppContent {
public:
    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_down(int local_x, int local_y) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;

private:
    // ── Render sections ─────────────────────────────────────────
    void render_header(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_process_list(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_system_stats(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);

    // ── State ───────────────────────────────────────────────────
    enum class Tab { Processes, System };
    Tab tab_ = Tab::Processes;
    int selected_pid_ = -1;
    float scroll_y_ = 0;
    int content_h_ = 0;
    SDL_Rect last_rect_ = {0, 0, 0, 0};
};
