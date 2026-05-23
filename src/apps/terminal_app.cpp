#include "terminal_app.h"
#include "terminal_shell.h"
#include "terminal_cmds_shell.h"
#include "terminal_cmds_file.h"
#include "terminal_cmds_text.h"
#include "terminal_cmds_system.h"
#include "terminal_cmds_net.h"
#include "terminal_cmds_archive.h"
#include "terminal_cmds_heros.h"
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
static const SDL_Color PROMPT_CLR = {100, 200, 255, 255};

// ── ANSI 8-color palette ────────────────────────────────────────

static const uint8_t ANSI_COLORS[8][3] = {
    {  0,   0,   0},  // 0: black
    {205,  49,  49},  // 1: red
    { 13, 188, 121},  // 2: green
    {229, 229,  16},  // 3: yellow
    { 36, 114, 200},  // 4: blue
    {188,  63, 188},  // 5: magenta
    { 17, 168, 205},  // 6: cyan
    {204, 204, 204},  // 7: white
};

static const uint8_t ANSI_BRIGHT[8][3] = {
    {102, 102, 102},  // 0: bright black (gray)
    {241,  76,  76},  // 1: bright red
    { 35, 209, 139},  // 2: bright green
    {245, 245,  67},  // 3: bright yellow
    { 59, 142, 234},  // 4: bright blue
    {214, 112, 214},  // 5: bright magenta
    { 41, 184, 219},  // 6: bright cyan
    {229, 229, 229},  // 7: bright white
};

// ── Constructor / Destructor ────────────────────────────────────

TerminalApp::TerminalApp() {
    // Load monospace font
    mono_font_ = TTF_OpenFont(MONO_FONT_PATH, MONO_FONT_SIZE);
    if (mono_font_) {
        TTF_SizeUTF8(mono_font_, "M", &char_w_, &char_h_);
    } else {
        fprintf(stderr, "TerminalApp: failed to load %s\n", MONO_FONT_PATH);
        char_w_ = 8;
        char_h_ = 15;
    }

    // Create shell engine
    shell_ = std::make_unique<ShellEngine>();

    // Register all commands
    register_shell_commands(*shell_);
    register_file_commands(*shell_);
    register_text_commands(*shell_);
    register_system_commands(*shell_);
    register_net_commands(*shell_);
    register_archive_commands(*shell_);
    register_heros_commands(*shell_);

    // Welcome message
    write_output("\033[1;36m");
    write_output("    _  _          ___  ___ \n");
    write_output("   | || |___ _ _ / _ \\/ __|\n");
    write_output("   | __ / -_) '_| (_) \\__ \\\n");
    write_output("   |_||_\\___|_|  \\___/|___/\n");
    write_output("\033[0m\n");
    write_output("\033[1mHerOS Terminal v0.1.0\033[0m — 100 built-in commands\n");
    write_output("Type '\033[1mhelp\033[0m' for a list of commands.\n\n");
}

TerminalApp::~TerminalApp() {
    if (mono_font_) TTF_CloseFont(mono_font_);
}

// ── Grid recalc ─────────────────────────────────────────────────

void TerminalApp::recalc_grid() {
    if (char_w_ > 0 && char_h_ > 0) {
        cols_ = std::max(20, (last_rect_.w - 16) / char_w_);
        rows_ = std::max(5, (last_rect_.h - 8) / char_h_);
    }
}

// ── Prompt ──────────────────────────────────────────────────────

std::string TerminalApp::get_prompt() const {
    auto& env = const_cast<ShellEngine*>(shell_.get())->env();
    std::string user = env.count("USER") ? env["USER"] : "hero";
    std::string host = env.count("HOSTNAME") ? env["HOSTNAME"] : "heros";
    std::string cwd = shell_->cwd();

    // Shorten home dir
    std::string home = env.count("HOME") ? env["HOME"] : "/home";
    if (cwd.find(home) == 0) {
        cwd = "~" + cwd.substr(home.size());
    }

    return "\033[1;32m" + user + "@" + host + "\033[0m:\033[1;34m" + cwd + "\033[0m$ ";
}

// ── ANSI SGR application ────────────────────────────────────────

void TerminalApp::apply_sgr(int code) {
    if (code == 0) { reset_style(); return; }
    if (code == 1) { current_style_.bold = true; return; }
    if (code == 2) { current_style_.dim = true; return; }
    if (code == 4) { current_style_.underline = true; return; }
    if (code == 22) { current_style_.bold = false; current_style_.dim = false; return; }
    if (code == 24) { current_style_.underline = false; return; }

    // Foreground 30-37
    if (code >= 30 && code <= 37) {
        int idx = code - 30;
        if (current_style_.bold) {
            current_style_.fg_r = ANSI_BRIGHT[idx][0];
            current_style_.fg_g = ANSI_BRIGHT[idx][1];
            current_style_.fg_b = ANSI_BRIGHT[idx][2];
        } else {
            current_style_.fg_r = ANSI_COLORS[idx][0];
            current_style_.fg_g = ANSI_COLORS[idx][1];
            current_style_.fg_b = ANSI_COLORS[idx][2];
        }
        return;
    }

    // Default foreground
    if (code == 39) {
        current_style_.fg_r = 204; current_style_.fg_g = 204; current_style_.fg_b = 204;
        return;
    }

    // Background 40-47
    if (code >= 40 && code <= 47) {
        int idx = code - 40;
        current_style_.bg_r = ANSI_COLORS[idx][0];
        current_style_.bg_g = ANSI_COLORS[idx][1];
        current_style_.bg_b = ANSI_COLORS[idx][2];
        current_style_.bg_a = 255;
        return;
    }

    // Default background
    if (code == 49) { current_style_.bg_a = 0; return; }

    // Bright foreground 90-97
    if (code >= 90 && code <= 97) {
        int idx = code - 90;
        current_style_.fg_r = ANSI_BRIGHT[idx][0];
        current_style_.fg_g = ANSI_BRIGHT[idx][1];
        current_style_.fg_b = ANSI_BRIGHT[idx][2];
        return;
    }

    // Bright background 100-107
    if (code >= 100 && code <= 107) {
        int idx = code - 100;
        current_style_.bg_r = ANSI_BRIGHT[idx][0];
        current_style_.bg_g = ANSI_BRIGHT[idx][1];
        current_style_.bg_b = ANSI_BRIGHT[idx][2];
        current_style_.bg_a = 255;
        return;
    }

    // Reverse video
    if (code == 7) {
        std::swap(current_style_.fg_r, current_style_.bg_r);
        std::swap(current_style_.fg_g, current_style_.bg_g);
        std::swap(current_style_.fg_b, current_style_.bg_b);
        current_style_.bg_a = 255;
        return;
    }
}

// ── Write output with ANSI parsing ──────────────────────────────

void TerminalApp::write_output(const std::string& text) {
    if (buffer_.empty()) buffer_.push_back(TermLine{});

    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];

        // ANSI escape sequence
        if (c == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
            i += 2; // skip ESC[
            std::vector<int> codes;
            std::string num;

            while (i < text.size()) {
                if (text[i] >= '0' && text[i] <= '9') {
                    num += text[i];
                } else if (text[i] == ';') {
                    codes.push_back(num.empty() ? 0 : std::stoi(num));
                    num.clear();
                } else {
                    // End of sequence
                    codes.push_back(num.empty() ? 0 : std::stoi(num));
                    if (text[i] == 'm') {
                        for (int code : codes) apply_sgr(code);
                    }
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        // Newline
        if (c == '\n') {
            buffer_.push_back(TermLine{});
            i++;
            continue;
        }

        // Carriage return
        if (c == '\r') {
            i++;
            continue;
        }

        // Tab
        if (c == '\t') {
            int spaces = 8 - ((int)buffer_.back().chars.size() % 8);
            for (int s = 0; s < spaces; s++) {
                buffer_.back().chars.push_back({' ', current_style_});
            }
            i++;
            continue;
        }

        // Regular character
        if (c >= 32 || (unsigned char)c >= 128) {
            buffer_.back().chars.push_back({c, current_style_});
        }
        i++;
    }

    // Trim buffer to max lines
    while ((int)buffer_.size() > max_lines_) {
        buffer_.erase(buffer_.begin());
    }
}

void TerminalApp::write_line(const std::string& text) {
    write_output(text + "\n");
}

// ── Execute input ───────────────────────────────────────────────

void TerminalApp::execute_input() {
    std::string input = input_line_;
    input_line_.clear();
    cursor_pos_ = 0;
    history_idx_ = -1;

    // Write prompt + input to buffer
    write_output(get_prompt() + input + "\n");

    // Add to history
    if (!input.empty()) {
        // Don't add duplicates
        if (history_.empty() || history_.back() != input) {
            history_.push_back(input);
        }
    }

    if (input.empty()) return;

    // Build command context
    CmdContext cmd_ctx{
        ctx_.fs,
        ctx_.pm,
        ctx_.registry,
        ctx_.wm,
        ctx_.notifications,
        ctx_.bus,
        ctx_.screen_w, ctx_.screen_h,
        ctx_.window_id,
        shell_->cwd(),
        shell_->env(),
        "",  // stdin
        ""   // stdout
    };

    int status = shell_->execute(input, cmd_ctx);

    // Handle special return codes
    if (status == -999) {
        // exit
        if (ctx_.wm) ctx_.request_close();
        return;
    }
    if (status == -998) {
        // clear
        buffer_.clear();
        scroll_offset_ = 0;
        return;
    }

    // Write output
    if (!cmd_ctx.stdout_data.empty()) {
        write_output(cmd_ctx.stdout_data);
        // Ensure output ends with newline
        if (!cmd_ctx.stdout_data.empty() && cmd_ctx.stdout_data.back() != '\n') {
            write_output("\n");
        }
    }

    scroll_offset_ = 0;
}

// ── Rendering ───────────────────────────────────────────────────

void TerminalApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_rect_ = cr;
    recalc_grid();
    SDL_Renderer* r = ctx.r;

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
    int visible_rows = (cr.h - pad_y * 2) / char_h_;
    if (visible_rows < 1) visible_rows = 1;

    // Calculate prompt for input line
    std::string raw_prompt = get_prompt();
    // Strip ANSI from prompt to get display length
    std::string display_prompt;
    {
        size_t j = 0;
        while (j < raw_prompt.size()) {
            if (raw_prompt[j] == '\033' && j + 1 < raw_prompt.size() && raw_prompt[j+1] == '[') {
                j += 2;
                while (j < raw_prompt.size() && raw_prompt[j] != 'm') j++;
                if (j < raw_prompt.size()) j++;
            } else {
                display_prompt += raw_prompt[j++];
            }
        }
    }

    // The input line takes one row at the bottom
    int buffer_rows = visible_rows - 1;
    if (buffer_rows < 0) buffer_rows = 0;

    // Calculate which buffer lines to show
    int total_lines = (int)buffer_.size();
    int start_line = total_lines - buffer_rows - scroll_offset_;
    if (start_line < 0) start_line = 0;
    int end_line = std::min(total_lines, start_line + buffer_rows);

    // Render buffer lines
    for (int li = start_line; li < end_line; li++) {
        int screen_row = li - start_line;
        int y = y0 + screen_row * char_h_;

        auto& line = buffer_[li];

        // Group same-style spans for efficient rendering
        size_t ci = 0;
        int x = x0;
        while (ci < line.chars.size()) {
            AnsiStyle style = line.chars[ci].style;

            // Collect characters with same style
            std::string span;
            while (ci < line.chars.size() && line.chars[ci].style == style) {
                span += line.chars[ci].ch;
                ci++;
            }

            // Draw background if non-transparent
            if (style.bg_a > 0) {
                int tw = 0;
                TTF_SizeUTF8(mono_font_, span.c_str(), &tw, nullptr);
                SDL_Rect bg = {x, y, tw, char_h_};
                SDL_SetRenderDrawColor(r, style.bg_r, style.bg_g, style.bg_b, style.bg_a);
                SDL_RenderFillRect(r, &bg);
            }

            // Draw text
            SDL_Color fg = {style.fg_r, style.fg_g, style.fg_b, 255};
            if (style.dim) {
                fg.r = fg.r * 2 / 3;
                fg.g = fg.g * 2 / 3;
                fg.b = fg.b * 2 / 3;
            }

            draw::text(r, mono_font_, span.c_str(), x, y, fg);

            int tw = 0;
            TTF_SizeUTF8(mono_font_, span.c_str(), &tw, nullptr);

            // Underline
            if (style.underline) {
                SDL_SetRenderDrawColor(r, fg.r, fg.g, fg.b, fg.a);
                SDL_RenderDrawLine(r, x, y + char_h_ - 2, x + tw, y + char_h_ - 2);
            }

            x += tw;
        }
    }

    // Render input line at bottom
    int input_y = y0 + buffer_rows * char_h_;

    // Render the prompt with ANSI colors
    {
        AnsiStyle prompt_style;
        int px = x0;
        size_t j = 0;
        while (j < raw_prompt.size()) {
            if (raw_prompt[j] == '\033' && j + 1 < raw_prompt.size() && raw_prompt[j+1] == '[') {
                j += 2;
                std::vector<int> codes;
                std::string num;
                while (j < raw_prompt.size()) {
                    if (raw_prompt[j] >= '0' && raw_prompt[j] <= '9') {
                        num += raw_prompt[j];
                    } else if (raw_prompt[j] == ';') {
                        codes.push_back(num.empty() ? 0 : std::stoi(num));
                        num.clear();
                    } else {
                        codes.push_back(num.empty() ? 0 : std::stoi(num));
                        if (raw_prompt[j] == 'm') {
                            AnsiStyle saved = current_style_;
                            current_style_ = prompt_style;
                            for (int code : codes) apply_sgr(code);
                            prompt_style = current_style_;
                            current_style_ = saved;
                        }
                        j++;
                        break;
                    }
                    j++;
                }
            } else {
                // Render character
                char buf[2] = {raw_prompt[j], 0};
                SDL_Color fg = {prompt_style.fg_r, prompt_style.fg_g, prompt_style.fg_b, 255};
                draw::text(r, mono_font_, buf, px, input_y, fg);
                int cw = 0;
                TTF_SizeUTF8(mono_font_, buf, &cw, nullptr);
                px += cw;
                j++;
            }
        }

        // Render input text
        if (!input_line_.empty()) {
            draw::text(r, mono_font_, input_line_.c_str(), px, input_y, FG_COLOR);
        }

        // Cursor
        if (cursor_visible_) {
            int cursor_x = px;
            if (cursor_pos_ > 0 && cursor_pos_ <= (int)input_line_.size()) {
                std::string before = input_line_.substr(0, cursor_pos_);
                int tw = 0;
                TTF_SizeUTF8(mono_font_, before.c_str(), &tw, nullptr);
                cursor_x = px + tw;
            }
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, CURSOR_CLR.r, CURSOR_CLR.g, CURSOR_CLR.b, CURSOR_CLR.a);
            SDL_Rect cur = {cursor_x, input_y, 2, char_h_};
            SDL_RenderFillRect(r, &cur);
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

// ── Input Events ────────────────────────────────────────────────

void TerminalApp::on_key_down(SDL_Keycode key) {
    cursor_blink_time_ = SDL_GetTicks();
    cursor_visible_ = true;

    SDL_Keymod mod = SDL_GetModState();

    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        execute_input();
        return;

    case SDLK_BACKSPACE:
        if (cursor_pos_ > 0 && cursor_pos_ <= (int)input_line_.size()) {
            input_line_.erase(cursor_pos_ - 1, 1);
            cursor_pos_--;
        }
        return;

    case SDLK_DELETE:
        if (cursor_pos_ < (int)input_line_.size()) {
            input_line_.erase(cursor_pos_, 1);
        }
        return;

    case SDLK_LEFT:
        if (cursor_pos_ > 0) cursor_pos_--;
        return;

    case SDLK_RIGHT:
        if (cursor_pos_ < (int)input_line_.size()) cursor_pos_++;
        return;

    case SDLK_HOME:
        cursor_pos_ = 0;
        return;

    case SDLK_END:
        cursor_pos_ = (int)input_line_.size();
        return;

    case SDLK_UP:
        // History navigation
        if (!history_.empty()) {
            if (history_idx_ == -1) {
                saved_input_ = input_line_;
                history_idx_ = (int)history_.size() - 1;
            } else if (history_idx_ > 0) {
                history_idx_--;
            }
            input_line_ = history_[history_idx_];
            cursor_pos_ = (int)input_line_.size();
        }
        return;

    case SDLK_DOWN:
        if (history_idx_ >= 0) {
            history_idx_++;
            if (history_idx_ >= (int)history_.size()) {
                history_idx_ = -1;
                input_line_ = saved_input_;
            } else {
                input_line_ = history_[history_idx_];
            }
            cursor_pos_ = (int)input_line_.size();
        }
        return;

    case SDLK_PAGEUP:
        scroll_offset_ += rows_ / 2;
        if (scroll_offset_ > (int)buffer_.size() - 1) scroll_offset_ = (int)buffer_.size() - 1;
        return;

    case SDLK_PAGEDOWN:
        scroll_offset_ -= rows_ / 2;
        if (scroll_offset_ < 0) scroll_offset_ = 0;
        return;

    case SDLK_TAB:
        // Simple tab completion: find commands starting with input
        if (!input_line_.empty()) {
            std::vector<std::string> matches;
            for (auto& [name, _] : shell_->all_commands()) {
                if (name.find(input_line_) == 0) matches.push_back(name);
            }
            if (matches.size() == 1) {
                input_line_ = matches[0] + " ";
                cursor_pos_ = (int)input_line_.size();
            } else if (matches.size() > 1) {
                std::sort(matches.begin(), matches.end());
                write_output(get_prompt() + input_line_ + "\n");
                std::string list;
                for (auto& m : matches) {
                    list += m + "  ";
                }
                write_line(list);
            }
        }
        return;

    case SDLK_c:
        if (mod & KMOD_CTRL) {
            write_output(get_prompt() + input_line_ + "^C\n");
            input_line_.clear();
            cursor_pos_ = 0;
            return;
        }
        break;

    case SDLK_l:
        if (mod & KMOD_CTRL) {
            buffer_.clear();
            scroll_offset_ = 0;
            return;
        }
        break;

    case SDLK_u:
        if (mod & KMOD_CTRL) {
            input_line_.erase(0, cursor_pos_);
            cursor_pos_ = 0;
            return;
        }
        break;

    case SDLK_k:
        if (mod & KMOD_CTRL) {
            input_line_.erase(cursor_pos_);
            return;
        }
        break;

    case SDLK_a:
        if (mod & KMOD_CTRL) {
            cursor_pos_ = 0;
            return;
        }
        break;

    case SDLK_e:
        if (mod & KMOD_CTRL) {
            cursor_pos_ = (int)input_line_.size();
            return;
        }
        break;

    case SDLK_w:
        if (mod & KMOD_CTRL) {
            // Delete previous word
            if (cursor_pos_ > 0) {
                int end = cursor_pos_;
                while (cursor_pos_ > 0 && input_line_[cursor_pos_ - 1] == ' ') cursor_pos_--;
                while (cursor_pos_ > 0 && input_line_[cursor_pos_ - 1] != ' ') cursor_pos_--;
                input_line_.erase(cursor_pos_, end - cursor_pos_);
            }
            return;
        }
        break;

    default:
        break;
    }
}

void TerminalApp::on_text_input(const char* text) {
    if (!text || !text[0]) return;

    // Don't insert control characters
    if ((unsigned char)text[0] < 32 && text[0] != '\t') return;

    input_line_.insert(cursor_pos_, text);
    cursor_pos_ += (int)strlen(text);
    scroll_offset_ = 0;
}

void TerminalApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x; (void)local_y;

    scroll_offset_ -= scroll_y * 3;
    if (scroll_offset_ < 0) scroll_offset_ = 0;
    int max_scroll = std::max(0, (int)buffer_.size() - 1);
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

HEROS_APP(TerminalApp, "com.heros.terminal", "Terminal", "0.1.0")
