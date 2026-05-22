#pragma once
#include "../window.h"
#include <string>
#include <vector>
#include <ctime>

// ── Data Model ──────────────────────────────────────────────────

struct JournalEntry {
    int id = 0;
    std::string title;
    std::string body;
    time_t created = 0;
    time_t updated = 0;
    bool favorite = false;
    bool archived = false;
    std::string filename; // on-disk filename (without path)
};

enum class ViewMode { List, Edit };
enum class NavTab { Entries, Favorites, Insights, Templates, Archive };

// Word-wrap helper: maps a display line back to its source line + char offset
struct WrapLine {
    int source_line = 0;
    int char_offset = 0;
    int char_count = 0;
};

struct Cursor {
    int line = 0;
    int col = 0;
};

// ── Journal App ─────────────────────────────────────────────────

class JournalApp : public AppContent {
public:
    JournalApp();

    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_down(int local_x, int local_y) override;
    void on_mouse_up(int local_x, int local_y) override;
    void on_mouse_move(int local_x, int local_y) override;
    void on_key_down(SDL_Keycode key) override;
    void on_text_input(const char* text) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;

private:
    // ── Persistence ────────────────────────────────────────────
    void ensure_data_dir();
    void load_all_entries();
    void save_entry(JournalEntry& entry);
    void delete_entry_file(const JournalEntry& entry);
    std::string data_dir() const;

    // ── Entry management ───────────────────────────────────────
    void new_entry(const std::string& title = "", const std::string& body = "");
    void open_entry(int index);
    void save_current();
    void delete_current();
    void go_back();

    // ── Text editing helpers ───────────────────────────────────
    void rebuild_lines();
    void rebuild_wraps(TTF_Font* font, int wrap_width);
    void insert_char(const char* text);
    void delete_at_cursor();
    void backspace_at_cursor();
    void split_line();
    void ensure_cursor_visible();
    int wrap_line_for_cursor() const;
    Cursor cursor_from_wrap_line(int wrap_idx, int col) const;
    int col_for_x(TTF_Font* font, int wrap_idx, int x) const;

    // ── Filtered views ─────────────────────────────────────────
    std::vector<int> filtered_indices() const;

    // ── Rendering sub-functions ────────────────────────────────
    void render_nav(const RenderCtx& ctx, int jx, int jy, int jh);
    void render_list_view(const RenderCtx& ctx, int x, int y, int w, int h);
    void render_edit_view(const RenderCtx& ctx, int x, int y, int w, int h);
    void render_insights_view(const RenderCtx& ctx, int x, int y, int w, int h);
    void render_templates_view(const RenderCtx& ctx, int x, int y, int w, int h);
    void render_toolbar(const RenderCtx& ctx, int x, int y, int w);

    // ── State ──────────────────────────────────────────────────
    std::vector<JournalEntry> entries_;
    ViewMode mode_ = ViewMode::List;
    NavTab active_tab_ = NavTab::Entries;
    int editing_index_ = -1; // index into entries_

    // Nav hover
    int hover_nav_ = -1;
    int nav_w_ = 110;

    // List view
    int list_scroll_ = 0;
    int hover_list_ = -1;

    // Editor state
    std::string edit_title_;
    std::vector<std::string> lines_; // body split by newline
    Cursor cursor_;
    std::vector<WrapLine> cached_wraps_;
    int scroll_offset_ = 0;
    int last_wrap_width_ = 0;
    bool editing_title_ = false; // true = cursor in title, false = cursor in body
    int title_cursor_col_ = 0;

    // Cursor blink
    Uint32 cursor_blink_time_ = 0;
    bool cursor_visible_ = true;

    // Toolbar hover
    int hover_tool_ = -1;

    // Layout cache
    SDL_Rect last_content_rect_ = {0, 0, 0, 0};
    const Fonts* cached_fonts_ = nullptr;
    int next_id_ = 1;
};
