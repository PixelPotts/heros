#pragma once
#include "../window.h"
#include <string>
#include <vector>

class SettingsApp : public AppContent {
public:
    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_down(int local_x, int local_y) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;

private:
    // ── Render sections ─────────────────────────────────────────
    void render_sidebar(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_general(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_display(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_notifications(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_about(SDL_Renderer* r, const Fonts* f, int x, int y, int w);

    // ── State ───────────────────────────────────────────────────
    enum class Page { General, Display, Notifications, About };
    Page page_ = Page::General;
    float scroll_y_ = 0;
    int content_h_ = 0;
    SDL_Rect last_rect_ = {0, 0, 0, 0};
};
