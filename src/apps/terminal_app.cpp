#include "terminal_app.h"
#include "terminal_pty.h"
#include "terminal_vt.h"
#include "../app_registry.h"
#include "../process.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

// ── Monospace font path ─────────────────────────────────────────

static const char* MONO_FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
static const int MONO_FONT_SIZE = 13;

// ── Terminal colors ─────────────────────────────────────────────

static const SDL_Color BG_COLOR   = {20, 22, 30, 245};
static const SDL_Color FG_COLOR   = {204, 204, 204, 255};
static const SDL_Color CURSOR_CLR = {100, 200, 100, 200};

// ── Constructor / Destructor ────────────────────────────────────

TerminalApp::TerminalApp() {
    mono_font_ = TTF_OpenFont(MONO_FONT_PATH, MONO_FONT_SIZE);
    if (mono_font_) {
        TTF_SizeUTF8(mono_font_, "M", &char_w_, &char_h_);
    } else {
        fprintf(stderr, "TerminalApp: failed to load %s\n", MONO_FONT_PATH);
        char_w_ = 8;
        char_h_ = 15;
    }

    vt_ = std::make_unique<VtScreen>(cols_, rows_);
    pty_ = std::make_unique<PtyBackend>();
    pty_->start(cols_, rows_);
}

TerminalApp::~TerminalApp() {
    pty_.reset();   // shutdown PTY first (kills child)
    vt_.reset();
    if (mono_font_) TTF_CloseFont(mono_font_);
}

// ── Grid recalc ─────────────────────────────────────────────────

void TerminalApp::recalc_grid() {
    if (char_w_ <= 0 || char_h_ <= 0) return;
    int new_cols = std::max(20, (last_rect_.w - 16) / char_w_);
    int new_rows = std::max(5, (last_rect_.h - 8) / char_h_);
    if (new_cols != cols_ || new_rows != rows_) {
        cols_ = new_cols;
        rows_ = new_rows;
        vt_->resize(cols_, rows_);
        pty_->set_size(cols_, rows_);
    }
}

// ── Poll PTY ────────────────────────────────────────────────────

void TerminalApp::poll_pty() {
    char buf[8192];
    for (;;) {
        int n = pty_->read_some(buf, (int)sizeof(buf));
        if (n <= 0) break;
        std::string response = vt_->process(buf, n);
        if (!response.empty()) {
            pty_->write_str(response);
        }
    }
}

// ── Rendering ───────────────────────────────────────────────────

void TerminalApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_rect_ = cr;
    recalc_grid();
    SDL_Renderer* r = ctx.r;

    // Poll for new data from the shell
    poll_pty();

    // Check if child died
    if (!pty_->is_alive()) {
        // Could auto-close or show message — for now just let the last output stay
    }

    // Cursor blink
    Uint32 now = SDL_GetTicks();
    if (now - cursor_blink_time_ >= (Uint32)CURSOR_BLINK_MS) {
        cursor_visible_ = !cursor_visible_;
        cursor_blink_time_ = now;
    }

    // Background
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, BG_COLOR.a);
    SDL_RenderFillRect(r, &cr);

    if (!mono_font_) return;

    int pad_x = 8;
    int pad_y = 4;
    int x0 = cr.x + pad_x;
    int y0 = cr.y + pad_y;

    // Determine which rows to render
    int visible_rows = rows_;

    // If scrolled back, show scrollback lines
    int sb_size = vt_->scrollback_size();
    int max_scroll = sb_size;
    if (scroll_offset_ > max_scroll) scroll_offset_ = max_scroll;

    // Render rows
    for (int screen_row = 0; screen_row < visible_rows; screen_row++) {
        int y = y0 + screen_row * char_h_;

        // Collect cells for this row
        const VtCell* cells = nullptr;
        int ncells = 0;

        if (scroll_offset_ > 0 && screen_row < scroll_offset_) {
            // This row comes from scrollback
            int sb_idx = sb_size - scroll_offset_ + screen_row;
            if (sb_idx >= 0 && sb_idx < sb_size) {
                auto& sb = vt_->scrollback();
                cells = sb[(size_t)sb_idx].data();
                ncells = (int)sb[(size_t)sb_idx].size();
            }
        } else {
            // From the live grid
            int grid_row = screen_row - scroll_offset_;
            if (grid_row >= 0 && grid_row < vt_->rows()) {
                // Use cell() accessor
                // We need to render cell by cell or span-group
                // Let's render span-grouped for efficiency
            }
        }

        // Render span-grouped text
        int x = x0;

        if (cells && ncells > 0) {
            // Scrollback row
            int ci = 0;
            while (ci < ncells) {
                const VtStyle& style = cells[ci].style;
                std::string span;
                while (ci < ncells && cells[ci].style == style) {
                    char32_t ch = cells[ci].ch;
                    if (ch < 128) {
                        span += (char)ch;
                    } else {
                        // UTF-8 encode
                        encode_utf8(span, ch);
                    }
                    ci++;
                }
                render_span(r, span, style, x, y);
            }
        } else if (scroll_offset_ == 0 || screen_row >= scroll_offset_) {
            int grid_row = screen_row - scroll_offset_;
            if (grid_row >= 0 && grid_row < vt_->rows()) {
                int ci = 0;
                while (ci < vt_->cols()) {
                    const VtStyle& style = vt_->cell(grid_row, ci).style;
                    std::string span;
                    while (ci < vt_->cols() && vt_->cell(grid_row, ci).style == style) {
                        char32_t ch = vt_->cell(grid_row, ci).ch;
                        if (ch < 128) {
                            span += (char)ch;
                        } else {
                            encode_utf8(span, ch);
                        }
                        ci++;
                    }
                    render_span(r, span, style, x, y);
                }
            }
        }
    }

    // Draw cursor (only when not scrolled back and cursor is visible from VT)
    if (scroll_offset_ == 0 && vt_->cursor_visible() && cursor_visible_) {
        int cx = x0 + vt_->cursor_col() * char_w_;
        int cy = y0 + vt_->cursor_row() * char_h_;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, CURSOR_CLR.r, CURSOR_CLR.g, CURSOR_CLR.b, CURSOR_CLR.a);
        SDL_Rect cur_rect = {cx, cy, char_w_, char_h_};
        SDL_RenderFillRect(r, &cur_rect);

        // Redraw the character under the cursor in dark color so it's visible
        if (vt_->cursor_row() < vt_->rows() && vt_->cursor_col() < vt_->cols()) {
            const VtCell& cc = vt_->cell(vt_->cursor_row(), vt_->cursor_col());
            if (cc.ch > 32) {
                std::string ch_str;
                if (cc.ch < 128) ch_str += (char)cc.ch;
                else encode_utf8(ch_str, cc.ch);
                SDL_Color dark = {20, 22, 30, 255};
                draw::text(r, mono_font_, ch_str.c_str(), cx, cy, dark);
            }
        }
    }

    // Scrollback indicator
    if (scroll_offset_ > 0) {
        char indicator[32];
        snprintf(indicator, sizeof(indicator), "[scroll: %d]", scroll_offset_);
        SDL_Color dim = {100, 100, 120, 200};
        draw::text_right(r, mono_font_, indicator, cr.x + cr.w - 8, cr.y + 4, dim);
    }
}

// ── Span rendering helper ───────────────────────────────────────

void TerminalApp::render_span(SDL_Renderer* r, const std::string& span,
                               const VtStyle& style, int& x, int y) {
    if (span.empty()) return;

    int tw = 0;
    TTF_SizeUTF8(mono_font_, span.c_str(), &tw, nullptr);

    // Background
    if (style.bg_a > 0) {
        SDL_SetRenderDrawColor(r, style.bg_r, style.bg_g, style.bg_b, style.bg_a);
        SDL_Rect bg = {x, y, tw, char_h_};
        SDL_RenderFillRect(r, &bg);
    }

    // Foreground
    SDL_Color fg = {style.fg_r, style.fg_g, style.fg_b, 255};
    if (style.dim) {
        fg.r = fg.r * 2 / 3;
        fg.g = fg.g * 2 / 3;
        fg.b = fg.b * 2 / 3;
    }
    if (!style.invisible) {
        draw::text(r, mono_font_, span.c_str(), x, y, fg);
    }

    // Underline
    if (style.underline) {
        SDL_SetRenderDrawColor(r, fg.r, fg.g, fg.b, fg.a);
        SDL_RenderDrawLine(r, x, y + char_h_ - 2, x + tw, y + char_h_ - 2);
    }

    // Strikethrough
    if (style.strikethrough) {
        SDL_SetRenderDrawColor(r, fg.r, fg.g, fg.b, fg.a);
        SDL_RenderDrawLine(r, x, y + char_h_ / 2, x + tw, y + char_h_ / 2);
    }

    x += tw;
}

// ── UTF-8 encode helper ────────────────────────────────────────

void TerminalApp::encode_utf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

// ── Key translation ─────────────────────────────────────────────

void TerminalApp::on_key_down(SDL_Keycode key) {
    cursor_blink_time_ = SDL_GetTicks();
    cursor_visible_ = true;

    SDL_Keymod mod = SDL_GetModState();
    bool ctrl = (mod & KMOD_CTRL) != 0;
    bool shift = (mod & KMOD_SHIFT) != 0;
    (void)shift;

    // When scrolled back, snap to bottom on any key except scroll keys
    if (scroll_offset_ > 0 && key != SDLK_PAGEUP && key != SDLK_PAGEDOWN) {
        scroll_offset_ = 0;
    }

    // Application cursor mode prefix
    const char* app_prefix = vt_->app_cursor_keys() ? "\033O" : "\033[";

    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        pty_->write_str("\r");
        return;

    case SDLK_BACKSPACE:
        pty_->write_str("\x7f");
        return;

    case SDLK_TAB:
        pty_->write_str("\t");
        return;

    case SDLK_ESCAPE:
        pty_->write_str("\033");
        return;

    case SDLK_UP:
        pty_->write_str(std::string(app_prefix) + "A");
        return;

    case SDLK_DOWN:
        pty_->write_str(std::string(app_prefix) + "B");
        return;

    case SDLK_RIGHT:
        pty_->write_str(std::string(app_prefix) + "C");
        return;

    case SDLK_LEFT:
        pty_->write_str(std::string(app_prefix) + "D");
        return;

    case SDLK_HOME:
        pty_->write_str("\033[H");
        return;

    case SDLK_END:
        pty_->write_str("\033[F");
        return;

    case SDLK_INSERT:
        pty_->write_str("\033[2~");
        return;

    case SDLK_DELETE:
        pty_->write_str("\033[3~");
        return;

    case SDLK_PAGEUP:
        if (shift) {
            // Shift+PageUp: scroll back
            scroll_offset_ += rows_ / 2;
            int max_scroll = vt_->scrollback_size();
            if (scroll_offset_ > max_scroll) scroll_offset_ = max_scroll;
            return;
        }
        pty_->write_str("\033[5~");
        return;

    case SDLK_PAGEDOWN:
        if (shift) {
            scroll_offset_ -= rows_ / 2;
            if (scroll_offset_ < 0) scroll_offset_ = 0;
            return;
        }
        pty_->write_str("\033[6~");
        return;

    // Function keys
    case SDLK_F1:  pty_->write_str("\033OP"); return;
    case SDLK_F2:  pty_->write_str("\033OQ"); return;
    case SDLK_F3:  pty_->write_str("\033OR"); return;
    case SDLK_F4:  pty_->write_str("\033OS"); return;
    case SDLK_F5:  pty_->write_str("\033[15~"); return;
    case SDLK_F6:  pty_->write_str("\033[17~"); return;
    case SDLK_F7:  pty_->write_str("\033[18~"); return;
    case SDLK_F8:  pty_->write_str("\033[19~"); return;
    case SDLK_F9:  pty_->write_str("\033[20~"); return;
    case SDLK_F10: pty_->write_str("\033[21~"); return;
    case SDLK_F11: pty_->write_str("\033[23~"); return;
    case SDLK_F12: pty_->write_str("\033[24~"); return;

    default:
        break;
    }

    // Ctrl+letter → control character
    if (ctrl && key >= SDLK_a && key <= SDLK_z) {
        char c = (char)(key - SDLK_a + 1);
        pty_->write_bytes(&c, 1);
        return;
    }

    // Ctrl+special
    if (ctrl) {
        switch (key) {
        case SDLK_BACKSLASH:  pty_->write_str("\x1c"); return;
        case SDLK_RIGHTBRACKET: pty_->write_str("\x1d"); return;
        case SDLK_LEFTBRACKET:  pty_->write_str("\x1b"); return;
        case SDLK_AT:         pty_->write_str("\x00"); return; // Ctrl+@
        default: break;
        }
    }
}

void TerminalApp::on_text_input(const char* text) {
    if (!text || !text[0]) return;

    // Don't insert if a Ctrl combo was handled
    SDL_Keymod mod = SDL_GetModState();
    if (mod & KMOD_CTRL) return;

    pty_->write_str(text);

    // Snap to bottom when typing
    if (scroll_offset_ > 0) scroll_offset_ = 0;
}

void TerminalApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x; (void)local_y;

    scroll_offset_ -= scroll_y * 3;
    if (scroll_offset_ < 0) scroll_offset_ = 0;
    int max_scroll = vt_->scrollback_size();
    if (scroll_offset_ > max_scroll) scroll_offset_ = max_scroll;
}

void TerminalApp::on_activate() {
    SDL_StartTextInput();
}

void TerminalApp::on_resize(int w, int h) {
    (void)w; (void)h;
    recalc_grid();
}

// ── HEROS_APP Macro ─────────────────────────────────────────────

HEROS_APP(TerminalApp, "com.heros.terminal", "Terminal", "0.2.0")
