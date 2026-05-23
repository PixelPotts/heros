#pragma once
#include "../heros_sdk.h"
#include "../ui.h"
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>
#include <memory>

// ── ANSI styling per character ──────────────────────────────────

struct AnsiStyle {
    uint8_t fg_r = 204, fg_g = 204, fg_b = 204;   // light gray default
    uint8_t bg_r = 0, bg_g = 0, bg_b = 0, bg_a = 0; // transparent bg
    bool bold = false;
    bool dim = false;
    bool underline = false;

    bool operator==(const AnsiStyle& o) const {
        return fg_r == o.fg_r && fg_g == o.fg_g && fg_b == o.fg_b
            && bg_r == o.bg_r && bg_g == o.bg_g && bg_b == o.bg_b && bg_a == o.bg_a
            && bold == o.bold && dim == o.dim && underline == o.underline;
    }
    bool operator!=(const AnsiStyle& o) const { return !(*this == o); }
};

// ── Styled character ────────────────────────────────────────────

struct StyledChar {
    char ch = ' ';
    AnsiStyle style;
};

// ── One line in the terminal buffer ─────────────────────────────

struct TermLine {
    std::vector<StyledChar> chars;

    std::string to_string() const {
        std::string s;
        s.reserve(chars.size());
        for (auto& sc : chars) s += sc.ch;
        // trim trailing spaces
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    }
};

// ── Forward declare shell engine ────────────────────────────────

class ShellEngine;

// ── Terminal App ────────────────────────────────────────────────

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

    // Buffer
    std::vector<TermLine> buffer_;
    int max_lines_ = 5000;
    int scroll_offset_ = 0; // lines scrolled back from bottom

    // Input
    std::string input_line_;
    int cursor_pos_ = 0;

    // History
    std::vector<std::string> history_;
    int history_idx_ = -1;
    std::string saved_input_; // saved when navigating history

    // Cursor blink
    Uint32 cursor_blink_time_ = 0;
    bool cursor_visible_ = true;
    static const int CURSOR_BLINK_MS = 530;

    // Shell engine
    std::unique_ptr<ShellEngine> shell_;

    // Display state
    SDL_Rect last_rect_ = {0, 0, 0, 0};
    int cols_ = 80;
    int rows_ = 24;

    // ── Methods ─────────────────────────────────────────────────

    void write_output(const std::string& text);
    void write_line(const std::string& text);
    void execute_input();
    void recalc_grid();
    std::string get_prompt() const;

    // ANSI parser state
    AnsiStyle current_style_;
    void reset_style() { current_style_ = AnsiStyle{}; }
    void apply_sgr(int code);
    void apply_sgr_color(const std::vector<int>& codes, size_t& i, bool foreground);
};
