#include "journal_app.h"
#include "../ui.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// ── Colors ──────────────────────────────────────────────────────

static const SDL_Color WHITE    = {230, 230, 240, 255};
static const SDL_Color DIM      = {150, 160, 180, 255};
static const SDL_Color ACCENT   = {100, 150, 255, 255};
static const SDL_Color BODY_CLR = {180, 185, 200, 220};
static const SDL_Color FAINT    = {180, 195, 220, 25};
static const SDL_Color STAR_CLR = {255, 200, 80, 255};

static const int NAV_W = 110;
static const int TOOLBAR_H = 32;
static const int LINE_H = 20;
static const int TITLE_H = 28;
static const int LIST_ITEM_H = 64;
static const int CURSOR_BLINK_MS = 530;

// ── Utility ─────────────────────────────────────────────────────

static std::string time_to_str(time_t t) {
    char buf[64];
    struct tm* tm = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    return buf;
}

static time_t str_to_time(const std::string& s) {
    struct tm tm = {};
    sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return mktime(&tm);
}

static std::string date_display(time_t t) {
    char buf[32];
    struct tm* tm = localtime(&t);
    strftime(buf, sizeof(buf), "%b %d, %Y", tm);
    return buf;
}

static int word_count(const std::string& s) {
    int count = 0;
    bool in_word = false;
    for (char c : s) {
        if (c == ' ' || c == '\n' || c == '\t') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

// ── Constructor ─────────────────────────────────────────────────

JournalApp::JournalApp() {
    ensure_data_dir();
    load_all_entries();
}

// ── Persistence ─────────────────────────────────────────────────

std::string JournalApp::data_dir() const {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.heros/journal";
}

void JournalApp::ensure_data_dir() {
    std::string home_dir = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/.heros";
    mkdir(home_dir.c_str(), 0755);
    mkdir(data_dir().c_str(), 0755);
}

void JournalApp::load_all_entries() {
    entries_.clear();
    std::string dir = data_dir();
    DIR* d = opendir(dir.c_str());
    if (!d) return;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name.size() < 5 || name.substr(name.size() - 4) != ".txt")
            continue;

        std::string path = dir + "/" + name;
        FILE* f = fopen(path.c_str(), "r");
        if (!f) continue;

        JournalEntry entry;
        entry.filename = name;

        // Parse header lines until "---"
        char line_buf[4096];
        bool in_body = false;
        std::string body;

        while (fgets(line_buf, sizeof(line_buf), f)) {
            std::string line = line_buf;
            // Strip trailing newline
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            if (!in_body) {
                if (line == "---") {
                    in_body = true;
                    continue;
                }
                if (line.substr(0, 7) == "TITLE: ")
                    entry.title = line.substr(7);
                else if (line.substr(0, 9) == "CREATED: ")
                    entry.created = str_to_time(line.substr(9));
                else if (line.substr(0, 9) == "UPDATED: ")
                    entry.updated = str_to_time(line.substr(9));
                else if (line.substr(0, 10) == "FAVORITE: ")
                    entry.favorite = (line.substr(10) == "1");
                else if (line.substr(0, 10) == "ARCHIVED: ")
                    entry.archived = (line.substr(10) == "1");
            } else {
                if (!body.empty()) body += "\n";
                body += line;
            }
        }
        entry.body = body;
        fclose(f);

        // Assign ID
        entry.id = next_id_++;
        entries_.push_back(std::move(entry));
    }
    closedir(d);

    // Sort by updated date, newest first
    std::sort(entries_.begin(), entries_.end(),
              [](const JournalEntry& a, const JournalEntry& b) {
                  return a.updated > b.updated;
              });
}

void JournalApp::save_entry(JournalEntry& entry) {
    ensure_data_dir();

    if (entry.filename.empty()) {
        // Generate filename from ID and timestamp
        char buf[64];
        snprintf(buf, sizeof(buf), "entry_%d_%ld.txt", entry.id, (long)entry.created);
        entry.filename = buf;
    }

    entry.updated = time(nullptr);

    std::string path = data_dir() + "/" + entry.filename;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        fprintf(stderr, "Failed to save journal entry: %s\n", path.c_str());
        return;
    }

    fprintf(f, "TITLE: %s\n", entry.title.c_str());
    fprintf(f, "CREATED: %s\n", time_to_str(entry.created).c_str());
    fprintf(f, "UPDATED: %s\n", time_to_str(entry.updated).c_str());
    fprintf(f, "FAVORITE: %d\n", entry.favorite ? 1 : 0);
    fprintf(f, "ARCHIVED: %d\n", entry.archived ? 1 : 0);
    fprintf(f, "---\n");
    fprintf(f, "%s\n", entry.body.c_str());
    fclose(f);
}

void JournalApp::delete_entry_file(const JournalEntry& entry) {
    if (entry.filename.empty()) return;
    std::string path = data_dir() + "/" + entry.filename;
    remove(path.c_str());
}

// ── Entry Management ────────────────────────────────────────────

void JournalApp::new_entry(const std::string& title, const std::string& body) {
    JournalEntry entry;
    entry.id = next_id_++;
    entry.title = title.empty() ? "Untitled" : title;
    entry.body = body;
    entry.created = time(nullptr);
    entry.updated = entry.created;
    if (active_tab_ == NavTab::Archive)
        entry.archived = true;

    save_entry(entry);
    entries_.insert(entries_.begin(), std::move(entry));
    open_entry(0);
}

void JournalApp::open_entry(int index) {
    if (index < 0 || index >= (int)entries_.size()) return;
    editing_index_ = index;
    edit_title_ = entries_[index].title;
    mode_ = ViewMode::Edit;
    editing_title_ = true;
    title_cursor_col_ = (int)edit_title_.size();
    rebuild_lines();
    cursor_ = {0, 0};
    scroll_offset_ = 0;
    cached_wraps_.clear();
    last_wrap_width_ = 0;
    cursor_blink_time_ = SDL_GetTicks();
    cursor_visible_ = true;
}

void JournalApp::save_current() {
    if (editing_index_ < 0 || editing_index_ >= (int)entries_.size()) return;

    auto& entry = entries_[editing_index_];
    entry.title = edit_title_;

    // Rebuild body from lines_
    std::string body;
    for (size_t i = 0; i < lines_.size(); i++) {
        if (i > 0) body += "\n";
        body += lines_[i];
    }
    entry.body = body;
    save_entry(entry);
}

void JournalApp::delete_current() {
    if (editing_index_ < 0 || editing_index_ >= (int)entries_.size()) return;
    delete_entry_file(entries_[editing_index_]);
    entries_.erase(entries_.begin() + editing_index_);
    editing_index_ = -1;
    mode_ = ViewMode::List;
}

void JournalApp::go_back() {
    save_current();
    mode_ = ViewMode::List;
    editing_index_ = -1;
}

// ── Text Editing ────────────────────────────────────────────────

void JournalApp::rebuild_lines() {
    lines_.clear();
    if (editing_index_ < 0 || editing_index_ >= (int)entries_.size()) {
        lines_.push_back("");
        return;
    }
    std::istringstream ss(entries_[editing_index_].body);
    std::string line;
    while (std::getline(ss, line))
        lines_.push_back(line);
    if (lines_.empty())
        lines_.push_back("");
}

void JournalApp::rebuild_wraps(TTF_Font* font, int wrap_width) {
    if (!font || wrap_width <= 0) return;
    if (wrap_width == last_wrap_width_ && !cached_wraps_.empty()) return;

    cached_wraps_.clear();
    last_wrap_width_ = wrap_width;

    for (int li = 0; li < (int)lines_.size(); li++) {
        const std::string& src = lines_[li];
        if (src.empty()) {
            cached_wraps_.push_back({li, 0, 0});
            continue;
        }

        int start = 0;
        while (start < (int)src.size()) {
            // Binary-search for how many chars fit in wrap_width
            int lo = 1, hi = (int)src.size() - start, best = hi;

            // First check if entire remaining line fits
            int tw = 0;
            TTF_SizeUTF8(font, src.substr(start, hi).c_str(), &tw, nullptr);
            if (tw <= wrap_width) {
                cached_wraps_.push_back({li, start, hi});
                break;
            }

            // Binary search for the max chars that fit
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                TTF_SizeUTF8(font, src.substr(start, mid).c_str(), &tw, nullptr);
                if (tw <= wrap_width) {
                    best = mid;
                    lo = mid + 1;
                } else {
                    hi = mid - 1;
                }
            }

            // Try to break at a space
            int break_at = best;
            for (int i = best; i > best / 2; i--) {
                if (src[start + i - 1] == ' ') {
                    break_at = i;
                    break;
                }
            }

            cached_wraps_.push_back({li, start, break_at});
            start += break_at;
        }
    }
}

void JournalApp::insert_char(const char* text) {
    if (editing_title_) {
        edit_title_.insert(title_cursor_col_, text);
        title_cursor_col_ += (int)strlen(text);
        return;
    }

    if (cursor_.line < 0 || cursor_.line >= (int)lines_.size()) return;
    auto& line = lines_[cursor_.line];
    int col = std::min(cursor_.col, (int)line.size());
    line.insert(col, text);
    cursor_.col = col + (int)strlen(text);
    cached_wraps_.clear();
    last_wrap_width_ = 0;
}

void JournalApp::backspace_at_cursor() {
    if (editing_title_) {
        if (title_cursor_col_ > 0) {
            edit_title_.erase(title_cursor_col_ - 1, 1);
            title_cursor_col_--;
        }
        return;
    }

    if (cursor_.col > 0) {
        auto& line = lines_[cursor_.line];
        int col = std::min(cursor_.col, (int)line.size());
        line.erase(col - 1, 1);
        cursor_.col = col - 1;
    } else if (cursor_.line > 0) {
        // Merge with previous line
        int prev_len = (int)lines_[cursor_.line - 1].size();
        lines_[cursor_.line - 1] += lines_[cursor_.line];
        lines_.erase(lines_.begin() + cursor_.line);
        cursor_.line--;
        cursor_.col = prev_len;
    }
    cached_wraps_.clear();
    last_wrap_width_ = 0;
}

void JournalApp::delete_at_cursor() {
    if (editing_title_) {
        if (title_cursor_col_ < (int)edit_title_.size()) {
            edit_title_.erase(title_cursor_col_, 1);
        }
        return;
    }

    auto& line = lines_[cursor_.line];
    int col = std::min(cursor_.col, (int)line.size());
    if (col < (int)line.size()) {
        line.erase(col, 1);
    } else if (cursor_.line + 1 < (int)lines_.size()) {
        // Merge with next line
        line += lines_[cursor_.line + 1];
        lines_.erase(lines_.begin() + cursor_.line + 1);
    }
    cached_wraps_.clear();
    last_wrap_width_ = 0;
}

void JournalApp::split_line() {
    if (editing_title_) {
        // Enter in title switches to body
        editing_title_ = false;
        cursor_ = {0, 0};
        return;
    }

    auto& line = lines_[cursor_.line];
    int col = std::min(cursor_.col, (int)line.size());
    std::string remainder = line.substr(col);
    line = line.substr(0, col);
    lines_.insert(lines_.begin() + cursor_.line + 1, remainder);
    cursor_.line++;
    cursor_.col = 0;
    cached_wraps_.clear();
    last_wrap_width_ = 0;
}

int JournalApp::wrap_line_for_cursor() const {
    for (int i = 0; i < (int)cached_wraps_.size(); i++) {
        auto& w = cached_wraps_[i];
        if (w.source_line == cursor_.line) {
            int end = w.char_offset + w.char_count;
            if (cursor_.col >= w.char_offset && cursor_.col <= end) {
                // If cursor is at the end of this wrap and there's a next wrap on same source line,
                // prefer the next wrap line
                if (cursor_.col == end && i + 1 < (int)cached_wraps_.size()
                    && cached_wraps_[i + 1].source_line == cursor_.line) {
                    continue;
                }
                return i;
            }
        }
    }
    // Fallback: last wrap line for cursor's source line
    for (int i = (int)cached_wraps_.size() - 1; i >= 0; i--) {
        if (cached_wraps_[i].source_line == cursor_.line) return i;
    }
    return 0;
}

Cursor JournalApp::cursor_from_wrap_line(int wrap_idx, int col) const {
    if (wrap_idx < 0 || wrap_idx >= (int)cached_wraps_.size())
        return {0, 0};
    auto& w = cached_wraps_[wrap_idx];
    int c = std::max(0, std::min(col, w.char_count));
    return {w.source_line, w.char_offset + c};
}

int JournalApp::col_for_x(TTF_Font* font, int wrap_idx, int x) const {
    if (wrap_idx < 0 || wrap_idx >= (int)cached_wraps_.size()) return 0;
    auto& w = cached_wraps_[wrap_idx];
    if (w.source_line >= (int)lines_.size()) return 0;

    const std::string& src = lines_[w.source_line];
    std::string sub = src.substr(w.char_offset, w.char_count);

    // Walk through characters measuring width
    int best = 0;
    for (int i = 1; i <= (int)sub.size(); i++) {
        int tw = 0;
        TTF_SizeUTF8(font, sub.substr(0, i).c_str(), &tw, nullptr);
        if (tw > x + 4) break; // past the click point
        best = i;
    }
    return w.char_offset + best;
}

void JournalApp::ensure_cursor_visible() {
    int wl = wrap_line_for_cursor();
    int cursor_y = wl * LINE_H;

    // Visible area height (account for title + gap)
    int vis_h = last_content_rect_.h - TOOLBAR_H - TITLE_H - 12;
    if (vis_h <= 0) vis_h = 200;

    if (cursor_y < scroll_offset_)
        scroll_offset_ = cursor_y;
    else if (cursor_y + LINE_H > scroll_offset_ + vis_h)
        scroll_offset_ = cursor_y + LINE_H - vis_h;

    if (scroll_offset_ < 0) scroll_offset_ = 0;
}

// ── Filtered Views ──────────────────────────────────────────────

std::vector<int> JournalApp::filtered_indices() const {
    std::vector<int> out;
    for (int i = 0; i < (int)entries_.size(); i++) {
        const auto& e = entries_[i];
        switch (active_tab_) {
        case NavTab::Entries:
            if (!e.archived) out.push_back(i);
            break;
        case NavTab::Favorites:
            if (e.favorite && !e.archived) out.push_back(i);
            break;
        case NavTab::Archive:
            if (e.archived) out.push_back(i);
            break;
        default:
            break;
        }
    }
    return out;
}

// ── Rendering ───────────────────────────────────────────────────

void JournalApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_content_rect_ = cr;
    SDL_Renderer* r = ctx.r;

    // Cursor blink
    Uint32 now = SDL_GetTicks();
    if (now - cursor_blink_time_ >= CURSOR_BLINK_MS) {
        cursor_visible_ = !cursor_visible_;
        cursor_blink_time_ = now;
    }

    cached_fonts_ = ctx.fonts;
    int jx = cr.x, jy = cr.y, jw = cr.w, jh = cr.h;

    // Left nav
    render_nav(ctx, jx, jy, jh);

    // Content area
    int cx = jx + NAV_W + 1;
    int cy = jy + 1;
    int cw = jw - NAV_W - 2;
    int ch = jh - 2;

    if (mode_ == ViewMode::Edit) {
        render_toolbar(ctx, cx, cy + ch - TOOLBAR_H, cw);
        render_edit_view(ctx, cx + 12, cy + 8, cw - 24, ch - TOOLBAR_H - 16);
    } else {
        // Non-edit tabs
        switch (active_tab_) {
        case NavTab::Insights:
            render_insights_view(ctx, cx + 12, cy + 8, cw - 24, ch - 16);
            break;
        case NavTab::Templates:
            render_templates_view(ctx, cx + 12, cy + 8, cw - 24, ch - 16);
            break;
        default:
            render_toolbar(ctx, cx, cy + ch - TOOLBAR_H, cw);
            render_list_view(ctx, cx + 12, cy + 8, cw - 24, ch - TOOLBAR_H - 16);
            break;
        }
    }

    (void)r;
}

void JournalApp::render_nav(const RenderCtx& ctx, int jx, int jy, int jh) {
    SDL_Renderer* r = ctx.r;

    SDL_Rect nav = {jx + 1, jy + 1, NAV_W, jh - 2};
    draw::filled_rounded_rect(r, nav, 0, {10, 12, 22, 60});
    draw::line(r, jx + NAV_W, jy + 1, jx + NAV_W, jy + jh, {180, 195, 220, 20});

    struct NavItem { const char* label; Icon icon; NavTab tab; };
    NavItem items[] = {
        {"Entries",   Icon::Journal, NavTab::Entries},
        {"Favorites", Icon::Star,    NavTab::Favorites},
        {"Insights",  Icon::Sparkle, NavTab::Insights},
        {"Templates", Icon::Grid,    NavTab::Templates},
        {"Archive",   Icon::Box,     NavTab::Archive},
    };

    int ny = jy + 10;
    for (int i = 0; i < 5; i++) {
        bool active = (items[i].tab == active_tab_);
        bool hovered = (i == hover_nav_) && !active;

        if (active) {
            SDL_Rect hi = {jx + 4, ny - 2, NAV_W - 6, 22};
            draw::filled_rounded_rect(r, hi, 4, {100, 150, 255, 25});
            draw::filled_circle(r, jx + 8, ny + 9, 2, ACCENT);
        } else if (hovered) {
            SDL_Rect hi = {jx + 4, ny - 2, NAV_W - 6, 22};
            draw::filled_rounded_rect(r, hi, 4, {100, 150, 255, 12});
        }

        SDL_Color ic = active ? ACCENT : DIM;
        draw::icon(r, items[i].icon, jx + 22, ny + 9, 12, ic);
        draw::text(r, ctx.fonts->small, items[i].label, jx + 34, ny + 2, active ? WHITE : DIM);
        ny += 26;
    }
}

void JournalApp::render_list_view(const RenderCtx& ctx, int x, int y, int w, int h) {
    SDL_Renderer* r = ctx.r;
    auto indices = filtered_indices();

    if (indices.empty()) {
        draw::text_centered(r, ctx.fonts->body, "No entries yet.",
                            x + w / 2, y + h / 2 - 10, DIM);
        draw::text_centered(r, ctx.fonts->small, "Click + to create one.",
                            x + w / 2, y + h / 2 + 10, {130, 140, 160, 180});
        return;
    }

    int iy = y - list_scroll_;
    for (int idx = 0; idx < (int)indices.size(); idx++) {
        int entry_y = iy + idx * LIST_ITEM_H;
        if (entry_y + LIST_ITEM_H < y) continue;
        if (entry_y > y + h) break;

        const auto& entry = entries_[indices[idx]];
        bool hovered = (idx == hover_list_);

        // Item background
        SDL_Rect item_rect = {x - 4, entry_y, w + 8, LIST_ITEM_H - 4};
        if (hovered) {
            draw::filled_rounded_rect(r, item_rect, 6, {100, 150, 255, 15});
        }

        // Title
        draw::text(r, ctx.fonts->title, entry.title.c_str(), x + 2, entry_y + 4, WHITE);

        // Date
        std::string date = date_display(entry.updated);
        draw::text(r, ctx.fonts->small, date.c_str(), x + 2, entry_y + 24, DIM);

        // Favorite star
        if (entry.favorite) {
            draw::icon(r, Icon::Star, x + w - 8, entry_y + 14, 12, STAR_CLR);
        }

        // Preview (first ~60 chars of body)
        std::string preview = entry.body.substr(0, 60);
        // Replace newlines with spaces
        for (char& c : preview) if (c == '\n') c = ' ';
        if (entry.body.size() > 60) preview += "...";
        draw::text(r, ctx.fonts->small, preview.c_str(), x + 2, entry_y + 40, {140, 150, 170, 180});

        // Separator
        draw::line(r, x, entry_y + LIST_ITEM_H - 5, x + w, entry_y + LIST_ITEM_H - 5, FAINT);
    }
}

void JournalApp::render_edit_view(const RenderCtx& ctx, int x, int y, int w, int h) {
    SDL_Renderer* r = ctx.r;
    TTF_Font* body_font = ctx.fonts->body;
    TTF_Font* title_font = ctx.fonts->title;

    // Title field
    SDL_Rect title_bg = {x - 4, y - 2, w + 8, TITLE_H};
    if (editing_title_) {
        draw::filled_rounded_rect(r, title_bg, 4, {100, 150, 255, 12});
    }
    draw::text(r, title_font, edit_title_.c_str(), x, y + 2, WHITE);

    // Title cursor
    if (editing_title_ && cursor_visible_ && mode_ == ViewMode::Edit) {
        int tw = 0;
        if (title_cursor_col_ > 0) {
            std::string sub = edit_title_.substr(0, title_cursor_col_);
            TTF_SizeUTF8(title_font, sub.c_str(), &tw, nullptr);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, ACCENT.r, ACCENT.g, ACCENT.b, 200);
        SDL_RenderDrawLine(r, x + tw, y + 2, x + tw, y + TITLE_H - 6);
    }

    // Separator
    int sep_y = y + TITLE_H + 2;
    draw::line(r, x, sep_y, x + w, sep_y, FAINT);

    // Body area
    int body_y = sep_y + 6;
    int body_h = h - TITLE_H - 8;

    // Rebuild word wraps
    rebuild_wraps(body_font, w);

    // Render wrapped lines with scroll
    int vis_start = body_y;
    int clip_bottom = body_y + body_h;

    for (int i = 0; i < (int)cached_wraps_.size(); i++) {
        int ly = vis_start + i * LINE_H - scroll_offset_;
        if (ly + LINE_H < vis_start) continue;
        if (ly > clip_bottom) break;

        auto& wl = cached_wraps_[i];
        if (wl.source_line < (int)lines_.size() && wl.char_count > 0) {
            std::string disp = lines_[wl.source_line].substr(wl.char_offset, wl.char_count);
            draw::text(r, body_font, disp.c_str(), x, ly, BODY_CLR);
        }

        // Draw cursor on this wrap line
        if (!editing_title_ && cursor_visible_ && mode_ == ViewMode::Edit) {
            if (wl.source_line == cursor_.line) {
                int c = cursor_.col;
                if (c >= wl.char_offset && c <= wl.char_offset + wl.char_count) {
                    // Check this is the right wrap segment
                    bool is_last_wrap_for_line = (i + 1 >= (int)cached_wraps_.size()
                        || cached_wraps_[i + 1].source_line != cursor_.line);
                    bool at_boundary = (c == wl.char_offset + wl.char_count);

                    if (!at_boundary || is_last_wrap_for_line) {
                        int tw = 0;
                        if (c > wl.char_offset) {
                            std::string sub = lines_[wl.source_line].substr(wl.char_offset, c - wl.char_offset);
                            TTF_SizeUTF8(body_font, sub.c_str(), &tw, nullptr);
                        }
                        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(r, ACCENT.r, ACCENT.g, ACCENT.b, 200);
                        SDL_RenderDrawLine(r, x + tw, ly, x + tw, ly + LINE_H - 2);
                    }
                }
            }
        }
    }
}

void JournalApp::render_insights_view(const RenderCtx& ctx, int x, int y, int w, int h) {
    SDL_Renderer* r = ctx.r;
    (void)h;

    draw::text(r, ctx.fonts->title, "Insights", x, y, WHITE);
    y += 32;

    // Total entries
    int total = 0, fav = 0, archived = 0, total_words = 0;
    time_t earliest = 0, latest = 0;
    for (const auto& e : entries_) {
        total++;
        if (e.favorite) fav++;
        if (e.archived) archived++;
        total_words += word_count(e.body) + word_count(e.title);
        if (earliest == 0 || e.created < earliest) earliest = e.created;
        if (e.updated > latest) latest = e.updated;
    }

    char buf[128];

    snprintf(buf, sizeof(buf), "Total Entries: %d", total);
    draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
    y += LINE_H + 4;

    snprintf(buf, sizeof(buf), "Favorites: %d", fav);
    draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
    y += LINE_H + 4;

    snprintf(buf, sizeof(buf), "Archived: %d", archived);
    draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
    y += LINE_H + 4;

    snprintf(buf, sizeof(buf), "Total Words: %d", total_words);
    draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
    y += LINE_H + 4;

    if (earliest > 0) {
        snprintf(buf, sizeof(buf), "First Entry: %s", date_display(earliest).c_str());
        draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
        y += LINE_H + 4;
    }

    if (latest > 0) {
        snprintf(buf, sizeof(buf), "Last Updated: %s", date_display(latest).c_str());
        draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
        y += LINE_H + 4;
    }

    if (total > 0) {
        int avg = total_words / total;
        snprintf(buf, sizeof(buf), "Avg Words/Entry: %d", avg);
        draw::text(r, ctx.fonts->body, buf, x, y, BODY_CLR);
    }

    (void)w;
}

void JournalApp::render_templates_view(const RenderCtx& ctx, int x, int y, int w, int h) {
    SDL_Renderer* r = ctx.r;
    (void)h;

    draw::text(r, ctx.fonts->title, "Templates", x, y, WHITE);
    y += 32;

    struct Template { const char* title; const char* body; };
    Template templates[] = {
        {"Morning Reflection",
         "Today, I choose patience and presence.\nEach moment offers its own wisdom."},
        {"Gratitude List",
         "Three things I'm grateful for today:\n1. \n2. \n3. "},
        {"Weekly Review",
         "What went well this week:\n\nWhat I'd like to improve:\n\nGoals for next week:\n"},
        {"Free Write",
         ""},
        {"Dream Journal",
         "Last night I dreamed about:\n\nEmotions felt:\n\nSymbols or recurring themes:\n"},
    };

    for (int i = 0; i < 5; i++) {
        SDL_Rect item = {x - 4, y, w + 8, 40};
        bool hovered = (hover_list_ == i);
        if (hovered) {
            draw::filled_rounded_rect(r, item, 6, {100, 150, 255, 15});
        }

        draw::icon(r, Icon::Grid, x + 8, y + 14, 12, ACCENT);
        draw::text(r, ctx.fonts->body, templates[i].title, x + 24, y + 6, WHITE);
        draw::text(r, ctx.fonts->small, "Click to create", x + 24, y + 22, DIM);

        draw::line(r, x, y + 38, x + w, y + 38, FAINT);
        y += 44;
    }
}

void JournalApp::render_toolbar(const RenderCtx& ctx, int x, int y, int w) {
    SDL_Renderer* r = ctx.r;

    draw::line(r, x, y, x + w, y, {180, 195, 220, 20});

    if (mode_ == ViewMode::List) {
        // "New Entry" button
        int bx = x + w - 28;
        int by = y + TOOLBAR_H / 2;
        bool hovered = (hover_tool_ == 0);
        if (hovered) {
            draw::filled_rounded_rect(r, {bx - 10, by - 10, 20, 20}, 4, {100, 150, 255, 25});
        }
        draw::icon(r, Icon::Pen, bx, by, 14, hovered ? ACCENT : DIM);
    } else {
        // Edit mode toolbar: Back, Save, Favorite, Archive, Delete
        struct ToolBtn { const char* label; Icon icon; };
        ToolBtn btns[] = {
            {"Back",     Icon::ChevronUp},
            {"Save",     Icon::Check},
            {"Favorite", Icon::Star},
            {"Archive",  Icon::Box},
            {"Delete",   Icon::Trash},
        };

        int tx = x + 8;
        for (int i = 0; i < 5; i++) {
            bool hovered = (hover_tool_ == i);
            int bx = tx;
            int by = y + TOOLBAR_H / 2;

            SDL_Color ic = hovered ? ACCENT : DIM;

            // Special coloring for favorite if entry is favorited
            if (i == 2 && editing_index_ >= 0 && editing_index_ < (int)entries_.size()
                && entries_[editing_index_].favorite) {
                ic = STAR_CLR;
            }

            if (hovered) {
                draw::filled_rounded_rect(r, {bx - 10, by - 10, 20, 20}, 4, {100, 150, 255, 25});
            }
            draw::icon(r, btns[i].icon, bx, by, 12, ic);
            tx += 28;
        }
    }
}

// ── Mouse Events ────────────────────────────────────────────────

void JournalApp::on_mouse_down(int local_x, int local_y) {
    // Reset cursor blink on click
    cursor_blink_time_ = SDL_GetTicks();
    cursor_visible_ = true;

    // Nav click
    if (local_x < NAV_W) {
        NavTab tabs[] = {NavTab::Entries, NavTab::Favorites, NavTab::Insights, NavTab::Templates, NavTab::Archive};
        int ny = 10;
        for (int i = 0; i < 5; i++) {
            if (local_y >= ny - 2 && local_y < ny + 22) {
                if (mode_ == ViewMode::Edit) save_current();
                mode_ = ViewMode::List;
                editing_index_ = -1;
                active_tab_ = tabs[i];
                list_scroll_ = 0;
                hover_list_ = -1;
                return;
            }
            ny += 26;
        }
        return;
    }

    int cx = NAV_W + 1;
    int cw = last_content_rect_.w - NAV_W - 2;
    int ch = last_content_rect_.h - 2;

    // Toolbar click
    int toolbar_y = ch - TOOLBAR_H;
    if (local_y >= toolbar_y && mode_ != ViewMode::Edit
        && active_tab_ != NavTab::Insights && active_tab_ != NavTab::Templates) {
        // New entry button (rightmost area)
        if (local_x >= cx + cw - 38) {
            new_entry();
            return;
        }
    }

    if (mode_ == ViewMode::Edit && local_y >= toolbar_y) {
        // Edit toolbar buttons
        int tx = cx + 8;
        for (int i = 0; i < 5; i++) {
            if (local_x >= tx - 14 && local_x < tx + 14) {
                switch (i) {
                case 0: go_back(); return;
                case 1: save_current(); return;
                case 2: // Toggle favorite
                    if (editing_index_ >= 0 && editing_index_ < (int)entries_.size()) {
                        entries_[editing_index_].favorite = !entries_[editing_index_].favorite;
                        save_current();
                    }
                    return;
                case 3: // Toggle archive
                    if (editing_index_ >= 0 && editing_index_ < (int)entries_.size()) {
                        entries_[editing_index_].archived = !entries_[editing_index_].archived;
                        save_current();
                        go_back();
                    }
                    return;
                case 4: delete_current(); return;
                }
            }
            tx += 28;
        }
        return;
    }

    // Templates click
    if (active_tab_ == NavTab::Templates && mode_ == ViewMode::List) {
        struct Template { const char* title; const char* body; };
        Template templates[] = {
            {"Morning Reflection", "Today, I choose patience and presence.\nEach moment offers its own wisdom."},
            {"Gratitude List", "Three things I'm grateful for today:\n1. \n2. \n3. "},
            {"Weekly Review", "What went well this week:\n\nWhat I'd like to improve:\n\nGoals for next week:\n"},
            {"Free Write", ""},
            {"Dream Journal", "Last night I dreamed about:\n\nEmotions felt:\n\nSymbols or recurring themes:\n"},
        };

        int iy = 8 + 32; // after "Templates" title
        int local_content_y = local_y - iy;
        if (local_content_y >= 0) {
            int idx = local_content_y / 44;
            if (idx >= 0 && idx < 5) {
                new_entry(templates[idx].title, templates[idx].body);
                return;
            }
        }
        return;
    }

    // Edit mode — click to place cursor
    if (mode_ == ViewMode::Edit) {
        int content_x = cx + 12;
        int body_start_y = 8 + TITLE_H + 8;

        // Check if clicking in title area
        if (local_y >= 6 && local_y < 6 + TITLE_H) {
            editing_title_ = true;
            // Measure title click position
            int click_x = local_x - content_x;
            title_cursor_col_ = 0;
            if (cached_fonts_) {
                for (int i = 1; i <= (int)edit_title_.size(); i++) {
                    int tw = 0;
                    TTF_SizeUTF8(cached_fonts_->title, edit_title_.substr(0, i).c_str(), &tw, nullptr);
                    if (tw > click_x + 4) break;
                    title_cursor_col_ = i;
                }
            }
            return;
        }

        // Body area click
        if (local_y >= body_start_y && !cached_wraps_.empty()) {
            editing_title_ = false;
            int relative_y = local_y - body_start_y + scroll_offset_;
            int wrap_idx = relative_y / LINE_H;
            if (wrap_idx < 0) wrap_idx = 0;
            if (wrap_idx >= (int)cached_wraps_.size())
                wrap_idx = (int)cached_wraps_.size() - 1;

            int click_x = local_x - content_x;
            int col = col_for_x(cached_fonts_ ? cached_fonts_->body : nullptr, wrap_idx, click_x);
            cursor_.line = cached_wraps_[wrap_idx].source_line;
            cursor_.col = col;
        }
        return;
    }

    // List mode — click to open entry
    if (mode_ == ViewMode::List) {
        auto indices = filtered_indices();
        int iy = 8 - list_scroll_;
        int click_idx = (local_y - iy) / LIST_ITEM_H;
        if (click_idx >= 0 && click_idx < (int)indices.size()) {
            open_entry(indices[click_idx]);
        }
    }
}

void JournalApp::on_mouse_up(int local_x, int local_y) {
    (void)local_x; (void)local_y;
}

void JournalApp::on_mouse_move(int local_x, int local_y) {
    hover_nav_ = -1;
    hover_tool_ = -1;
    hover_list_ = -1;

    // Nav
    if (local_x < NAV_W) {
        int ny = 10;
        for (int i = 0; i < 5; i++) {
            if (local_y >= ny - 2 && local_y < ny + 22) {
                hover_nav_ = i;
                return;
            }
            ny += 26;
        }
        return;
    }

    int cx = NAV_W + 1;
    int cw = last_content_rect_.w - NAV_W - 2;
    int ch = last_content_rect_.h - 2;
    int toolbar_y = ch - TOOLBAR_H;

    // Toolbar hover
    if (local_y >= toolbar_y) {
        if (mode_ == ViewMode::List) {
            if (local_x >= cx + cw - 38)
                hover_tool_ = 0;
        } else {
            int tx = cx + 8;
            for (int i = 0; i < 5; i++) {
                if (local_x >= tx - 14 && local_x < tx + 14) {
                    hover_tool_ = i;
                    return;
                }
                tx += 28;
            }
        }
        return;
    }

    // List/template hover
    if (mode_ == ViewMode::List) {
        if (active_tab_ == NavTab::Templates) {
            int iy = 8 + 32;
            int relative_y = local_y - iy;
            if (relative_y >= 0) {
                int idx = relative_y / 44;
                if (idx >= 0 && idx < 5)
                    hover_list_ = idx;
            }
        } else if (active_tab_ != NavTab::Insights) {
            auto indices = filtered_indices();
            int iy = 8 - list_scroll_;
            int idx = (local_y - iy) / LIST_ITEM_H;
            if (idx >= 0 && idx < (int)indices.size())
                hover_list_ = idx;
        }
    }
}

// ── Keyboard Events ─────────────────────────────────────────────

void JournalApp::on_key_down(SDL_Keycode key) {
    // Reset cursor blink on keypress
    cursor_blink_time_ = SDL_GetTicks();
    cursor_visible_ = true;

    if (mode_ == ViewMode::Edit) {
        switch (key) {
        case SDLK_ESCAPE:
            go_back();
            return;

        case SDLK_TAB:
            editing_title_ = !editing_title_;
            if (editing_title_) {
                title_cursor_col_ = (int)edit_title_.size();
            } else {
                cursor_ = {0, 0};
            }
            return;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            split_line();
            ensure_cursor_visible();
            return;

        case SDLK_BACKSPACE:
            backspace_at_cursor();
            ensure_cursor_visible();
            return;

        case SDLK_DELETE:
            delete_at_cursor();
            return;

        case SDLK_LEFT:
            if (editing_title_) {
                if (title_cursor_col_ > 0) title_cursor_col_--;
            } else {
                if (cursor_.col > 0) {
                    cursor_.col--;
                } else if (cursor_.line > 0) {
                    cursor_.line--;
                    cursor_.col = (int)lines_[cursor_.line].size();
                }
                ensure_cursor_visible();
            }
            return;

        case SDLK_RIGHT:
            if (editing_title_) {
                if (title_cursor_col_ < (int)edit_title_.size()) title_cursor_col_++;
            } else {
                if (cursor_.col < (int)lines_[cursor_.line].size()) {
                    cursor_.col++;
                } else if (cursor_.line + 1 < (int)lines_.size()) {
                    cursor_.line++;
                    cursor_.col = 0;
                }
                ensure_cursor_visible();
            }
            return;

        case SDLK_UP:
            if (!editing_title_ && !cached_wraps_.empty()) {
                int wl = wrap_line_for_cursor();
                if (wl > 0) {
                    // Move to previous wrap line, try to keep similar column
                    auto& prev = cached_wraps_[wl - 1];
                    int local_col = cursor_.col - cached_wraps_[wl].char_offset;
                    int new_col = std::min(local_col, prev.char_count);
                    cursor_.line = prev.source_line;
                    cursor_.col = prev.char_offset + new_col;
                }
                ensure_cursor_visible();
            }
            return;

        case SDLK_DOWN:
            if (!editing_title_ && !cached_wraps_.empty()) {
                int wl = wrap_line_for_cursor();
                if (wl + 1 < (int)cached_wraps_.size()) {
                    auto& next = cached_wraps_[wl + 1];
                    int local_col = cursor_.col - cached_wraps_[wl].char_offset;
                    int new_col = std::min(local_col, next.char_count);
                    cursor_.line = next.source_line;
                    cursor_.col = next.char_offset + new_col;
                }
                ensure_cursor_visible();
            }
            return;

        case SDLK_HOME:
            if (editing_title_) {
                title_cursor_col_ = 0;
            } else {
                cursor_.col = 0;
                ensure_cursor_visible();
            }
            return;

        case SDLK_END:
            if (editing_title_) {
                title_cursor_col_ = (int)edit_title_.size();
            } else {
                if (cursor_.line < (int)lines_.size())
                    cursor_.col = (int)lines_[cursor_.line].size();
                ensure_cursor_visible();
            }
            return;

        default:
            break;
        }
    }
}

void JournalApp::on_text_input(const char* text) {
    if (mode_ != ViewMode::Edit) return;
    insert_char(text);
    if (!editing_title_)
        ensure_cursor_visible();
}

void JournalApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x; (void)local_y;

    if (mode_ == ViewMode::Edit) {
        scroll_offset_ -= scroll_y * LINE_H * 3;
        if (scroll_offset_ < 0) scroll_offset_ = 0;

        int max_scroll = (int)cached_wraps_.size() * LINE_H - 100;
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_offset_ > max_scroll) scroll_offset_ = max_scroll;
    } else {
        list_scroll_ -= scroll_y * LIST_ITEM_H;
        if (list_scroll_ < 0) list_scroll_ = 0;

        auto indices = filtered_indices();
        int max_scroll = (int)indices.size() * LIST_ITEM_H - 100;
        if (max_scroll < 0) max_scroll = 0;
        if (list_scroll_ > max_scroll) list_scroll_ = max_scroll;
    }
}
