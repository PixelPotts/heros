#pragma once
// ── VT100/xterm Screen Buffer + CSI Escape Parser ───────────────
// Implements a full terminal screen buffer with scrollback, alternate
// screen, scroll regions, and the essential CSI/OSC escape sequences
// needed for bash, vim, htop, less, and modern CLI tools.

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

// ── ANSI 8-color palettes ───────────────────────────────────────

static const uint8_t VT_COLORS[8][3] = {
    {  0,   0,   0},  // black
    {205,  49,  49},  // red
    { 13, 188, 121},  // green
    {229, 229,  16},  // yellow
    { 36, 114, 200},  // blue
    {188,  63, 188},  // magenta
    { 17, 168, 205},  // cyan
    {204, 204, 204},  // white
};

static const uint8_t VT_BRIGHT[8][3] = {
    {102, 102, 102},  // bright black
    {241,  76,  76},  // bright red
    { 35, 209, 139},  // bright green
    {245, 245,  67},  // bright yellow
    { 59, 142, 234},  // bright blue
    {214, 112, 214},  // bright magenta
    { 41, 184, 219},  // bright cyan
    {229, 229, 229},  // bright white
};

// ── Per-cell style ──────────────────────────────────────────────

struct VtStyle {
    uint8_t fg_r = 204, fg_g = 204, fg_b = 204;
    uint8_t bg_r = 0, bg_g = 0, bg_b = 0, bg_a = 0;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool reverse = false;
    bool invisible = false;
    bool strikethrough = false;

    bool operator==(const VtStyle& o) const {
        return fg_r == o.fg_r && fg_g == o.fg_g && fg_b == o.fg_b
            && bg_r == o.bg_r && bg_g == o.bg_g && bg_b == o.bg_b && bg_a == o.bg_a
            && bold == o.bold && dim == o.dim && italic == o.italic
            && underline == o.underline && reverse == o.reverse
            && invisible == o.invisible && strikethrough == o.strikethrough;
    }
    bool operator!=(const VtStyle& o) const { return !(*this == o); }
};

// ── Per-cell data ───────────────────────────────────────────────

struct VtCell {
    char32_t ch = ' ';
    VtStyle style;
};

// ── One row ────────────────────────────────────────────────────

using VtRow = std::vector<VtCell>;

static inline VtRow make_row(int cols) {
    return VtRow((size_t)cols, VtCell{' ', VtStyle{}});
}

// ── Parser states ──────────────────────────────────────────────

enum class VtState {
    Ground,
    Escape,
    Csi,
    CsiParam,
    CsiIntermediate,
    Osc,
    OscString,
    Utf8,
    EscHash,
    DcsEntry,
};

// ── VtScreen ────────────────────────────────────────────────────

class VtScreen {
public:
    VtScreen(int cols, int rows)
        : cols_(cols), rows_(rows)
    {
        init_grid(grid_, cols, rows);
        init_grid(alt_grid_, cols, rows);
    }

    // ── Public access ───────────────────────────────────────────

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    const VtCell& cell(int row, int col) const {
        static VtCell blank;
        if (row < 0 || row >= rows_ || col < 0 || col >= cols_) return blank;
        return grid_[(size_t)row][(size_t)col];
    }

    int cursor_row() const { return cur_r_; }
    int cursor_col() const { return cur_c_; }
    bool cursor_visible() const { return cursor_visible_; }
    const std::string& title() const { return title_; }
    bool using_alt_screen() const { return alt_active_; }

    // Scrollback
    const std::vector<VtRow>& scrollback() const { return scrollback_; }
    int scrollback_size() const { return (int)scrollback_.size(); }

    // ── Process raw bytes from PTY ──────────────────────────────

    // Returns a response string to write back (for DSR/DA queries)
    std::string process(const char* data, int len) {
        response_.clear();
        for (int i = 0; i < len; i++) {
            process_byte((uint8_t)data[i]);
        }
        return response_;
    }

    // ── Resize ──────────────────────────────────────────────────

    void resize(int new_cols, int new_rows) {
        if (new_cols == cols_ && new_rows == rows_) return;
        resize_grid(grid_, cols_, rows_, new_cols, new_rows);
        resize_grid(alt_grid_, cols_, rows_, new_cols, new_rows);
        cols_ = new_cols;
        rows_ = new_rows;
        cur_r_ = std::min(cur_r_, rows_ - 1);
        cur_c_ = std::min(cur_c_, cols_ - 1);
        scroll_top_ = 0;
        scroll_bot_ = rows_ - 1;
    }

private:
    int cols_, rows_;
    int cur_r_ = 0, cur_c_ = 0;
    bool cursor_visible_ = true;
    VtStyle style_;
    std::string title_;

    // Grids
    std::vector<VtRow> grid_;
    std::vector<VtRow> alt_grid_;
    bool alt_active_ = false;

    // Scroll region
    int scroll_top_ = 0;
    int scroll_bot_ = 0; // initialized in ctor body via rows_-1

    // Scrollback (only for primary screen)
    std::vector<VtRow> scrollback_;
    static const int MAX_SCROLLBACK = 10000;

    // Saved cursor
    int saved_r_ = 0, saved_c_ = 0;
    VtStyle saved_style_;

    // Alt screen saved cursor
    int alt_saved_r_ = 0, alt_saved_c_ = 0;
    VtStyle alt_saved_style_;

    // Parser state
    VtState state_ = VtState::Ground;
    std::vector<int> params_;
    std::string intermediate_;
    std::string osc_string_;
    std::string response_;

    // UTF-8 state
    char32_t utf8_cp_ = 0;
    int utf8_remaining_ = 0;

    // Modes
    bool app_cursor_keys_ = false;
    bool bracketed_paste_ = false;
    bool origin_mode_ = false;
    bool auto_wrap_ = true;
    bool wrap_pending_ = false;  // deferred wrap at right margin

    // ── Grid helpers ────────────────────────────────────────────

    static void init_grid(std::vector<VtRow>& g, int cols, int rows) {
        g.resize((size_t)rows);
        for (auto& row : g) row = make_row(cols);
    }

    static void resize_grid(std::vector<VtRow>& g, int /*old_cols*/, int old_rows,
                             int new_cols, int new_rows) {
        // Adjust existing rows width
        for (auto& row : g) {
            row.resize((size_t)new_cols, VtCell{' ', VtStyle{}});
        }
        // Add/remove rows
        if (new_rows > old_rows) {
            for (int i = old_rows; i < new_rows; i++)
                g.push_back(make_row(new_cols));
        } else if (new_rows < old_rows) {
            g.resize((size_t)new_rows);
        }
    }

    // ── Scroll ──────────────────────────────────────────────────

    void scroll_up(int n = 1) {
        for (int i = 0; i < n; i++) {
            // If scrolling the full screen and on primary buffer, save to scrollback
            if (!alt_active_ && scroll_top_ == 0) {
                scrollback_.push_back(std::move(grid_[0]));
                if ((int)scrollback_.size() > MAX_SCROLLBACK)
                    scrollback_.erase(scrollback_.begin());
            }
            grid_.erase(grid_.begin() + scroll_top_);
            grid_.insert(grid_.begin() + scroll_bot_, make_row(cols_));
        }
    }

    void scroll_down(int n = 1) {
        for (int i = 0; i < n; i++) {
            grid_.erase(grid_.begin() + scroll_bot_);
            grid_.insert(grid_.begin() + scroll_top_, make_row(cols_));
        }
    }

    // ── Cursor movement helpers ─────────────────────────────────

    void advance_cursor() {
        if (wrap_pending_) {
            wrap_pending_ = false;
            cur_c_ = 0;
            if (cur_r_ == scroll_bot_) {
                scroll_up();
            } else if (cur_r_ < rows_ - 1) {
                cur_r_++;
            }
        }
    }

    void put_char(char32_t ch) {
        advance_cursor();

        if (cur_c_ >= cols_) cur_c_ = cols_ - 1;
        if (cur_r_ >= rows_) cur_r_ = rows_ - 1;

        grid_[(size_t)cur_r_][(size_t)cur_c_].ch = ch;
        grid_[(size_t)cur_r_][(size_t)cur_c_].style = effective_style();

        if (cur_c_ < cols_ - 1) {
            cur_c_++;
        } else {
            if (auto_wrap_) {
                wrap_pending_ = true;
            }
        }
    }

    VtStyle effective_style() const {
        VtStyle s = style_;
        if (s.reverse) {
            std::swap(s.fg_r, s.bg_r);
            std::swap(s.fg_g, s.bg_g);
            std::swap(s.fg_b, s.bg_b);
            s.bg_a = 255;
        }
        return s;
    }

    // ── Erase helpers ───────────────────────────────────────────

    void erase_cell(int r, int c) {
        if (r >= 0 && r < rows_ && c >= 0 && c < cols_) {
            grid_[(size_t)r][(size_t)c] = VtCell{' ', effective_style()};
        }
    }

    void erase_row(int r, int c_start, int c_end) {
        for (int c = c_start; c <= c_end && c < cols_; c++)
            erase_cell(r, c);
    }

    // ── process_byte: top-level parser ──────────────────────────

    void process_byte(uint8_t b) {
        // UTF-8 continuation
        if (state_ == VtState::Utf8) {
            if ((b & 0xC0) == 0x80) {
                utf8_cp_ = (utf8_cp_ << 6) | (b & 0x3F);
                utf8_remaining_--;
                if (utf8_remaining_ == 0) {
                    state_ = VtState::Ground;
                    put_char(utf8_cp_);
                }
            } else {
                // Broken UTF-8 — treat as latin-1
                state_ = VtState::Ground;
                process_byte(b);
            }
            return;
        }

        switch (state_) {
        case VtState::Ground:
            ground_byte(b);
            break;
        case VtState::Escape:
            escape_byte(b);
            break;
        case VtState::Csi:
        case VtState::CsiParam:
        case VtState::CsiIntermediate:
            csi_byte(b);
            break;
        case VtState::Osc:
        case VtState::OscString:
            osc_byte(b);
            break;
        case VtState::EscHash:
            esc_hash_byte(b);
            break;
        case VtState::DcsEntry:
            // Consume until ST
            if (b == 0x1B) state_ = VtState::Escape;
            else if (b == 0x9C) state_ = VtState::Ground;
            break;
        default:
            state_ = VtState::Ground;
            break;
        }
    }

    // ── Ground state ────────────────────────────────────────────

    void ground_byte(uint8_t b) {
        // C0 controls
        if (b < 0x20) {
            switch (b) {
            case 0x07: /* BEL — ignore */ break;
            case 0x08: /* BS  */ if (cur_c_ > 0) { cur_c_--; wrap_pending_ = false; } break;
            case 0x09: /* HT  */ cur_c_ = std::min(cols_ - 1, (cur_c_ / 8 + 1) * 8); wrap_pending_ = false; break;
            case 0x0A: /* LF  */
            case 0x0B: /* VT  */
            case 0x0C: /* FF  */
                wrap_pending_ = false;
                if (cur_r_ == scroll_bot_) {
                    scroll_up();
                } else if (cur_r_ < rows_ - 1) {
                    cur_r_++;
                }
                break;
            case 0x0D: /* CR  */ cur_c_ = 0; wrap_pending_ = false; break;
            case 0x1B: /* ESC */ state_ = VtState::Escape; break;
            default: break;
            }
            return;
        }

        // UTF-8 lead bytes
        if (b >= 0xC0) {
            if (b < 0xE0)      { utf8_cp_ = b & 0x1F; utf8_remaining_ = 1; }
            else if (b < 0xF0) { utf8_cp_ = b & 0x0F; utf8_remaining_ = 2; }
            else               { utf8_cp_ = b & 0x07; utf8_remaining_ = 3; }
            state_ = VtState::Utf8;
            return;
        }

        // Printable ASCII
        put_char((char32_t)b);
    }

    // ── Escape state ────────────────────────────────────────────

    void escape_byte(uint8_t b) {
        state_ = VtState::Ground;
        switch (b) {
        case '[':
            state_ = VtState::Csi;
            params_.clear();
            intermediate_.clear();
            break;
        case ']':
            state_ = VtState::Osc;
            osc_string_.clear();
            break;
        case '(':
        case ')':
        case '*':
        case '+':
            // Charset designation — consume next byte
            // We ignore it, but need to eat the parameter byte
            // We'll just go back to ground and let the next byte be handled
            break;
        case '#':
            state_ = VtState::EscHash;
            break;
        case '7': // DECSC — save cursor
            saved_r_ = cur_r_; saved_c_ = cur_c_; saved_style_ = style_;
            break;
        case '8': // DECRC — restore cursor
            cur_r_ = saved_r_; cur_c_ = saved_c_; style_ = saved_style_;
            break;
        case 'D': // IND — index (scroll up if at bottom)
            if (cur_r_ == scroll_bot_) scroll_up();
            else if (cur_r_ < rows_ - 1) cur_r_++;
            break;
        case 'M': // RI — reverse index
            if (cur_r_ == scroll_top_) scroll_down();
            else if (cur_r_ > 0) cur_r_--;
            break;
        case 'E': // NEL — next line
            cur_c_ = 0;
            if (cur_r_ == scroll_bot_) scroll_up();
            else if (cur_r_ < rows_ - 1) cur_r_++;
            break;
        case 'c': // RIS — full reset
            full_reset();
            break;
        case 'P': // DCS
            state_ = VtState::DcsEntry;
            break;
        case '\\': // ST (end of OSC/DCS) — already handled by going to ground
            break;
        default:
            break;
        }
    }

    // ── Esc # state ─────────────────────────────────────────────

    void esc_hash_byte(uint8_t b) {
        state_ = VtState::Ground;
        if (b == '8') {
            // DECALN — fill screen with 'E' for alignment test
            for (int r = 0; r < rows_; r++)
                for (int c = 0; c < cols_; c++)
                    grid_[(size_t)r][(size_t)c] = VtCell{'E', VtStyle{}};
        }
    }

    // ── CSI state ───────────────────────────────────────────────

    void csi_byte(uint8_t b) {
        // C0 controls inside CSI
        if (b < 0x20) {
            ground_byte(b);
            return;
        }

        // Parameter bytes: 0x30-0x3F
        if (b >= 0x30 && b <= 0x3F) {
            intermediate_ += (char)b;
            state_ = VtState::CsiParam;
            return;
        }

        // Intermediate bytes: 0x20-0x2F
        if (b >= 0x20 && b <= 0x2F) {
            intermediate_ += (char)b;
            state_ = VtState::CsiIntermediate;
            return;
        }

        // Final byte: 0x40-0x7E — dispatch
        if (b >= 0x40 && b <= 0x7E) {
            parse_params();
            csi_dispatch((char)b);
            state_ = VtState::Ground;
            return;
        }

        // Unexpected
        state_ = VtState::Ground;
    }

    void parse_params() {
        params_.clear();
        std::string num;
        bool is_private = false;

        for (char c : intermediate_) {
            if (c == '?') { is_private = true; continue; }
            if (c == '>') { is_private = true; continue; }
            if (c >= '0' && c <= '9') {
                num += c;
            } else if (c == ';') {
                params_.push_back(num.empty() ? 0 : std::stoi(num));
                num.clear();
            } else if (c == ':') {
                // Sub-parameters (for SGR colons) — treat as semicolons
                params_.push_back(num.empty() ? 0 : std::stoi(num));
                num.clear();
            }
        }
        params_.push_back(num.empty() ? 0 : std::stoi(num));

        if (is_private && !params_.empty()) {
            // Mark as private by negating first param... actually, use a flag
            params_.insert(params_.begin(), -1); // sentinel for '?'
        }
    }

    int param(int idx, int def = 0) const {
        if (idx < 0 || idx >= (int)params_.size()) return def;
        return params_[(size_t)idx] == 0 ? def : params_[(size_t)idx];
    }

    bool is_private_mode() const {
        return !params_.empty() && params_[0] == -1;
    }

    int pparam(int idx, int def = 0) const {
        // For private mode, offset by 1 (skip the sentinel)
        return param(idx + 1, def);
    }

    // ── CSI dispatch ────────────────────────────────────────────

    void csi_dispatch(char final_ch) {
        if (is_private_mode()) {
            csi_dispatch_private(final_ch);
            return;
        }

        switch (final_ch) {
        case 'A': // CUU — cursor up
            cur_r_ = std::max(scroll_top_, cur_r_ - param(0, 1));
            wrap_pending_ = false;
            break;

        case 'B': // CUD — cursor down
            cur_r_ = std::min(scroll_bot_, cur_r_ + param(0, 1));
            wrap_pending_ = false;
            break;

        case 'C': // CUF — cursor forward
            cur_c_ = std::min(cols_ - 1, cur_c_ + param(0, 1));
            wrap_pending_ = false;
            break;

        case 'D': // CUB — cursor back
            cur_c_ = std::max(0, cur_c_ - param(0, 1));
            wrap_pending_ = false;
            break;

        case 'E': // CNL — cursor next line
            cur_c_ = 0;
            cur_r_ = std::min(scroll_bot_, cur_r_ + param(0, 1));
            wrap_pending_ = false;
            break;

        case 'F': // CPL — cursor previous line
            cur_c_ = 0;
            cur_r_ = std::max(scroll_top_, cur_r_ - param(0, 1));
            wrap_pending_ = false;
            break;

        case 'G': // CHA — cursor horizontal absolute
            cur_c_ = std::clamp(param(0, 1) - 1, 0, cols_ - 1);
            wrap_pending_ = false;
            break;

        case 'H': // CUP — cursor position
        case 'f': // HVP — same as CUP
            cur_r_ = std::clamp(param(0, 1) - 1, 0, rows_ - 1);
            cur_c_ = std::clamp(param(1, 1) - 1, 0, cols_ - 1);
            wrap_pending_ = false;
            break;

        case 'J': // ED — erase display
            erase_display(param(0, 0));
            break;

        case 'K': // EL — erase line
            erase_line(param(0, 0));
            break;

        case 'L': // IL — insert lines
            insert_lines(param(0, 1));
            break;

        case 'M': // DL — delete lines
            delete_lines(param(0, 1));
            break;

        case 'P': // DCH — delete characters
            delete_chars(param(0, 1));
            break;

        case 'S': // SU — scroll up
            scroll_up(param(0, 1));
            break;

        case 'T': // SD — scroll down
            scroll_down(param(0, 1));
            break;

        case 'X': // ECH — erase characters
            {
                int n = param(0, 1);
                for (int i = 0; i < n && cur_c_ + i < cols_; i++)
                    erase_cell(cur_r_, cur_c_ + i);
            }
            break;

        case '@': // ICH — insert characters
            insert_chars(param(0, 1));
            break;

        case 'd': // VPA — vertical position absolute
            cur_r_ = std::clamp(param(0, 1) - 1, 0, rows_ - 1);
            wrap_pending_ = false;
            break;

        case 'm': // SGR — select graphic rendition
            sgr();
            break;

        case 'n': // DSR — device status report
            if (param(0) == 6) {
                // Report cursor position
                char buf[32];
                snprintf(buf, sizeof(buf), "\033[%d;%dR", cur_r_ + 1, cur_c_ + 1);
                response_ += buf;
            } else if (param(0) == 5) {
                response_ += "\033[0n"; // terminal OK
            }
            break;

        case 'c': // DA — device attributes
            response_ += "\033[?62;22c"; // VT220
            break;

        case 'r': // DECSTBM — set scrolling region
            {
                int top = param(0, 1) - 1;
                int bot = param(1, rows_) - 1;
                top = std::clamp(top, 0, rows_ - 1);
                bot = std::clamp(bot, 0, rows_ - 1);
                if (top < bot) {
                    scroll_top_ = top;
                    scroll_bot_ = bot;
                }
                cur_r_ = origin_mode_ ? scroll_top_ : 0;
                cur_c_ = 0;
                wrap_pending_ = false;
            }
            break;

        case 's': // SCP — save cursor position
            saved_r_ = cur_r_; saved_c_ = cur_c_;
            break;

        case 'u': // RCP — restore cursor position
            cur_r_ = saved_r_; cur_c_ = saved_c_;
            wrap_pending_ = false;
            break;

        case 't': // Window manipulation — mostly ignore
            break;

        case 'h': // SM — set mode
            set_mode(false, true);
            break;

        case 'l': // RM — reset mode
            set_mode(false, false);
            break;

        default:
            break;
        }
    }

    // ── Private mode CSI (ESC [ ? ...) ──────────────────────────

    void csi_dispatch_private(char final_ch) {
        switch (final_ch) {
        case 'h': set_mode(true, true); break;
        case 'l': set_mode(true, false); break;
        case 'n': // DECDSR
            if (pparam(0) == 6) {
                char buf[32];
                snprintf(buf, sizeof(buf), "\033[?%d;%dR", cur_r_ + 1, cur_c_ + 1);
                response_ += buf;
            }
            break;
        case 'c': // DA
            response_ += "\033[?62;22c";
            break;
        default: break;
        }
    }

    // ── Mode set/reset ──────────────────────────────────────────

    void set_mode(bool is_private, bool enable) {
        if (is_private) {
            for (size_t i = 1; i < params_.size(); i++) {
                int mode = params_[i];
                switch (mode) {
                case 1:    // DECCKM — application cursor keys
                    app_cursor_keys_ = enable;
                    break;
                case 6:    // DECOM — origin mode
                    origin_mode_ = enable;
                    if (enable) { cur_r_ = scroll_top_; cur_c_ = 0; }
                    break;
                case 7:    // DECAWM — auto-wrap
                    auto_wrap_ = enable;
                    break;
                case 12:   // Cursor blink — ignore
                    break;
                case 25:   // DECTCEM — cursor visibility
                    cursor_visible_ = enable;
                    break;
                case 47:   // Alt screen (without save cursor)
                    if (enable) enter_alt_screen(false);
                    else leave_alt_screen(false);
                    break;
                case 1000: // Mouse tracking — ignore for now
                case 1002:
                case 1003:
                case 1006:
                case 1015:
                    break;
                case 1049: // Alt screen + save cursor
                    if (enable) enter_alt_screen(true);
                    else leave_alt_screen(true);
                    break;
                case 2004: // Bracketed paste
                    bracketed_paste_ = enable;
                    break;
                default:
                    break;
                }
            }
        } else {
            // Standard modes
            for (size_t i = 0; i < params_.size(); i++) {
                int mode = params_[i];
                switch (mode) {
                case 4: // IRM — insert mode (TODO)
                    break;
                case 20: // LNM — linefeed mode
                    break;
                default: break;
                }
            }
        }
    }

    // ── Alternate screen ────────────────────────────────────────

    void enter_alt_screen(bool save_cursor) {
        if (alt_active_) return;
        if (save_cursor) {
            alt_saved_r_ = cur_r_;
            alt_saved_c_ = cur_c_;
            alt_saved_style_ = style_;
        }
        std::swap(grid_, alt_grid_);
        alt_active_ = true;
        // Clear alt screen
        for (auto& row : grid_)
            row = make_row(cols_);
        cur_r_ = 0; cur_c_ = 0;
        scroll_top_ = 0;
        scroll_bot_ = rows_ - 1;
    }

    void leave_alt_screen(bool restore_cursor) {
        if (!alt_active_) return;
        std::swap(grid_, alt_grid_);
        alt_active_ = false;
        if (restore_cursor) {
            cur_r_ = alt_saved_r_;
            cur_c_ = alt_saved_c_;
            style_ = alt_saved_style_;
        }
        scroll_top_ = 0;
        scroll_bot_ = rows_ - 1;
    }

    // ── SGR — select graphic rendition ──────────────────────────

    void sgr() {
        if (params_.empty() || (params_.size() == 1 && params_[0] == 0)) {
            style_ = VtStyle{};
            return;
        }

        for (size_t i = 0; i < params_.size(); i++) {
            int code = params_[i];

            switch (code) {
            case 0:  style_ = VtStyle{}; break;
            case 1:  style_.bold = true; break;
            case 2:  style_.dim = true; break;
            case 3:  style_.italic = true; break;
            case 4:  style_.underline = true; break;
            case 7:  style_.reverse = true; break;
            case 8:  style_.invisible = true; break;
            case 9:  style_.strikethrough = true; break;
            case 21: style_.underline = true; break;  // double underline → underline
            case 22: style_.bold = false; style_.dim = false; break;
            case 23: style_.italic = false; break;
            case 24: style_.underline = false; break;
            case 27: style_.reverse = false; break;
            case 28: style_.invisible = false; break;
            case 29: style_.strikethrough = false; break;

            // Foreground 30-37
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                set_fg_indexed(code - 30);
                break;

            case 38: // Extended foreground
                parse_extended_color(i, true);
                break;

            case 39: // Default foreground
                style_.fg_r = 204; style_.fg_g = 204; style_.fg_b = 204;
                break;

            // Background 40-47
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                set_bg_indexed(code - 40);
                break;

            case 48: // Extended background
                parse_extended_color(i, false);
                break;

            case 49: // Default background
                style_.bg_r = 0; style_.bg_g = 0; style_.bg_b = 0; style_.bg_a = 0;
                break;

            // Bright foreground 90-97
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                set_fg_bright(code - 90);
                break;

            // Bright background 100-107
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                set_bg_bright(code - 100);
                break;

            default: break;
            }
        }
    }

    void set_fg_indexed(int idx) {
        const uint8_t* c = style_.bold ? VT_BRIGHT[idx] : VT_COLORS[idx];
        style_.fg_r = c[0]; style_.fg_g = c[1]; style_.fg_b = c[2];
    }

    void set_fg_bright(int idx) {
        style_.fg_r = VT_BRIGHT[idx][0];
        style_.fg_g = VT_BRIGHT[idx][1];
        style_.fg_b = VT_BRIGHT[idx][2];
    }

    void set_bg_indexed(int idx) {
        style_.bg_r = VT_COLORS[idx][0];
        style_.bg_g = VT_COLORS[idx][1];
        style_.bg_b = VT_COLORS[idx][2];
        style_.bg_a = 255;
    }

    void set_bg_bright(int idx) {
        style_.bg_r = VT_BRIGHT[idx][0];
        style_.bg_g = VT_BRIGHT[idx][1];
        style_.bg_b = VT_BRIGHT[idx][2];
        style_.bg_a = 255;
    }

    // ── 256-color / truecolor ───────────────────────────────────

    void parse_extended_color(size_t& i, bool fg) {
        if (i + 1 >= params_.size()) return;
        int mode = params_[i + 1];

        if (mode == 5 && i + 2 < params_.size()) {
            // 256-color
            int idx = params_[i + 2];
            i += 2;
            uint8_t r, g, b;
            color_256(idx, r, g, b);
            if (fg) { style_.fg_r = r; style_.fg_g = g; style_.fg_b = b; }
            else    { style_.bg_r = r; style_.bg_g = g; style_.bg_b = b; style_.bg_a = 255; }
        } else if (mode == 2 && i + 4 < params_.size()) {
            // Truecolor
            uint8_t r = (uint8_t)params_[i + 2];
            uint8_t g = (uint8_t)params_[i + 3];
            uint8_t b = (uint8_t)params_[i + 4];
            i += 4;
            if (fg) { style_.fg_r = r; style_.fg_g = g; style_.fg_b = b; }
            else    { style_.bg_r = r; style_.bg_g = g; style_.bg_b = b; style_.bg_a = 255; }
        }
    }

    static void color_256(int idx, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (idx < 8) {
            r = VT_COLORS[idx][0]; g = VT_COLORS[idx][1]; b = VT_COLORS[idx][2];
        } else if (idx < 16) {
            r = VT_BRIGHT[idx - 8][0]; g = VT_BRIGHT[idx - 8][1]; b = VT_BRIGHT[idx - 8][2];
        } else if (idx < 232) {
            // 6x6x6 color cube
            int v = idx - 16;
            int ri = v / 36;
            int gi = (v % 36) / 6;
            int bi = v % 6;
            r = (uint8_t)(ri ? 55 + ri * 40 : 0);
            g = (uint8_t)(gi ? 55 + gi * 40 : 0);
            b = (uint8_t)(bi ? 55 + bi * 40 : 0);
        } else {
            // Grayscale ramp
            uint8_t v = (uint8_t)(8 + (idx - 232) * 10);
            r = g = b = v;
        }
    }

    // ── Erase operations ────────────────────────────────────────

    void erase_display(int mode) {
        switch (mode) {
        case 0: // Below
            erase_row(cur_r_, cur_c_, cols_ - 1);
            for (int r = cur_r_ + 1; r < rows_; r++)
                erase_row(r, 0, cols_ - 1);
            break;
        case 1: // Above
            erase_row(cur_r_, 0, cur_c_);
            for (int r = 0; r < cur_r_; r++)
                erase_row(r, 0, cols_ - 1);
            break;
        case 2: // Entire screen
        case 3: // Entire screen + scrollback
            for (int r = 0; r < rows_; r++)
                erase_row(r, 0, cols_ - 1);
            if (mode == 3) scrollback_.clear();
            break;
        }
    }

    void erase_line(int mode) {
        switch (mode) {
        case 0: erase_row(cur_r_, cur_c_, cols_ - 1); break;
        case 1: erase_row(cur_r_, 0, cur_c_); break;
        case 2: erase_row(cur_r_, 0, cols_ - 1); break;
        }
    }

    // ── Insert/delete ───────────────────────────────────────────

    void insert_lines(int n) {
        if (cur_r_ < scroll_top_ || cur_r_ > scroll_bot_) return;
        n = std::min(n, scroll_bot_ - cur_r_ + 1);
        for (int i = 0; i < n; i++) {
            if (scroll_bot_ < (int)grid_.size())
                grid_.erase(grid_.begin() + scroll_bot_);
            grid_.insert(grid_.begin() + cur_r_, make_row(cols_));
        }
    }

    void delete_lines(int n) {
        if (cur_r_ < scroll_top_ || cur_r_ > scroll_bot_) return;
        n = std::min(n, scroll_bot_ - cur_r_ + 1);
        for (int i = 0; i < n; i++) {
            grid_.erase(grid_.begin() + cur_r_);
            grid_.insert(grid_.begin() + scroll_bot_, make_row(cols_));
        }
    }

    void insert_chars(int n) {
        auto& row = grid_[(size_t)cur_r_];
        n = std::min(n, cols_ - cur_c_);
        for (int i = 0; i < n; i++) {
            row.insert(row.begin() + cur_c_, VtCell{' ', effective_style()});
            if ((int)row.size() > cols_) row.pop_back();
        }
    }

    void delete_chars(int n) {
        auto& row = grid_[(size_t)cur_r_];
        n = std::min(n, cols_ - cur_c_);
        row.erase(row.begin() + cur_c_, row.begin() + cur_c_ + n);
        while ((int)row.size() < cols_)
            row.push_back(VtCell{' ', effective_style()});
    }

    // ── OSC ─────────────────────────────────────────────────────

    void osc_byte(uint8_t b) {
        if (b == 0x07) {
            // BEL terminates OSC
            osc_dispatch();
            state_ = VtState::Ground;
            return;
        }
        if (b == 0x1B) {
            // ESC — could be ST (\033\\)
            state_ = VtState::Escape;
            osc_dispatch();
            return;
        }
        if (b == 0x9C) {
            osc_dispatch();
            state_ = VtState::Ground;
            return;
        }
        osc_string_ += (char)b;
        state_ = VtState::OscString;
    }

    void osc_dispatch() {
        // Parse "code;string"
        auto semi = osc_string_.find(';');
        if (semi == std::string::npos) return;
        std::string code_str = osc_string_.substr(0, semi);
        std::string value = osc_string_.substr(semi + 1);
        int code = 0;
        for (char c : code_str) {
            if (c >= '0' && c <= '9') code = code * 10 + (c - '0');
        }
        switch (code) {
        case 0:  // Set icon name + title
        case 2:  // Set title
            title_ = value;
            break;
        case 1:  // Set icon name — ignore
            break;
        default:
            break;
        }
    }

    // ── Full reset ──────────────────────────────────────────────

    void full_reset() {
        style_ = VtStyle{};
        cur_r_ = 0; cur_c_ = 0;
        cursor_visible_ = true;
        scroll_top_ = 0;
        scroll_bot_ = rows_ - 1;
        app_cursor_keys_ = false;
        bracketed_paste_ = false;
        origin_mode_ = false;
        auto_wrap_ = true;
        wrap_pending_ = false;
        for (auto& row : grid_)
            row = make_row(cols_);
        if (alt_active_) {
            std::swap(grid_, alt_grid_);
            alt_active_ = false;
        }
    }

public:
    // Expose mode flags for key translation
    bool app_cursor_keys() const { return app_cursor_keys_; }
    bool bracketed_paste() const { return bracketed_paste_; }
};
