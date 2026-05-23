#pragma once
#include "../window.h"
#include <string>
#include <vector>

class FinanceApp : public AppContent {
public:
    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_move(int local_x, int local_y) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;

private:
    // ── Render sections ─────────────────────────────────────────
    void render_header(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_summary_row(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_spending_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_expenses_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_accounts_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_budget_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_transactions_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_goals_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);

    // ── State ───────────────────────────────────────────────────
    float scroll_y_ = 0;
    int content_h_ = 0;
    SDL_Rect last_rect_ = {0, 0, 0, 0};
};
