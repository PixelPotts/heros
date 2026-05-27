#pragma once
#include "../heros_sdk.h"
#include "../ui.h"
#include <SDL2/SDL_ttf.h>
#include <memory>

class PtyBackend;
class VtScreen;

// ── Terminal App (PTY-backed) ───────────────────────────────────

class TerminalApp : public AppContent {
public:
    TerminalApp();
    ~TerminalApp() override;

    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_key_down(SDL_Keycode key) override;
    void on_text_input(const char* text) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;
    void on_activate() override;
    void on_resize(int w, int h) override;

private:
    // Font
    TTF_Font* mono_font_ = nullptr;
    int char_w_ = 8;
    int char_h_ = 15;

    // Grid dimensions
    SDL_Rect last_rect_ = {0, 0, 0, 0};
    int cols_ = 80;
    int rows_ = 24;

    // PTY + VT emulation
    std::unique_ptr<PtyBackend> pty_;
    std::unique_ptr<VtScreen> vt_;

    // Scrollback browsing
    int scroll_offset_ = 0;

    // Cursor blink
    Uint32 cursor_blink_time_ = 0;
    bool cursor_visible_ = true;
    static const int CURSOR_BLINK_MS = 530;

    // Helpers
    void recalc_grid();
    void poll_pty();
};
