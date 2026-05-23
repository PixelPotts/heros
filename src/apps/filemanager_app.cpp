#include "filemanager_app.h"
#include "../ui.h"
#include <cstdio>
#include <ctime>
#include <algorithm>

// ── Constants ───────────────────────────────────────────────────

static const int PAD = 16;
static const int GAP = 12;
static const int ROW_H = 30;

// ── Colors ──────────────────────────────────────────────────────

static const SDL_Color CARD_BG = {22, 27, 55, 200};
static const SDL_Color WHITE   = {230, 230, 240, 255};
static const SDL_Color DIM     = {150, 160, 180, 255};
static const SDL_Color FAINT   = {255, 255, 255, 20};
static const SDL_Color ACCENT  = {100, 150, 255, 255};
static const SDL_Color FOLDER_COL = {230, 200, 60, 255};

// ── Helpers ─────────────────────────────────────────────────────

static void card_bg(SDL_Renderer* r, int x, int y, int w, int h) {
    draw::filled_rounded_rect(r, {x, y, w, h}, 8, CARD_BG);
}

static std::string format_size(uint64_t bytes) {
    char buf[32];
    if (bytes < 1024)
        snprintf(buf, sizeof(buf), "%lu B", (unsigned long)bytes);
    else if (bytes < 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

static std::string format_time(time_t t) {
    if (t == 0) return "--";
    struct tm* tm = localtime(&t);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min);
    return buf;
}

// ── Navigation ──────────────────────────────────────────────────

void FileManagerApp::navigate(const std::string& path) {
    current_path_ = path;
    needs_refresh_ = true;
    selected_index_ = -1;
    scroll_y_ = 0;
}

Icon FileManagerApp::icon_for_file(const FileInfo& fi) const {
    if (fi.is_directory) return Icon::Box;
    if (fi.mime_type.find("text") != std::string::npos) return Icon::Journal;
    if (fi.mime_type.find("image") != std::string::npos) return Icon::Image;
    return Icon::Grid;
}

// ── Main render ─────────────────────────────────────────────────

void FileManagerApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_rect_ = cr;
    SDL_Renderer* r = ctx.r;
    const Fonts* f = ctx.fonts;

    // Refresh file list if needed
    if (needs_refresh_ && ctx_.fs) {
        entries_ = ctx_.fs->list(current_path_);
        needs_refresh_ = false;
    }

    SDL_RenderSetClipRect(r, &cr);

    int x = cr.x + PAD;
    int w = cr.w - PAD * 2;
    int y = cr.y;

    render_toolbar(r, f, x, y, w);
    y += 36;

    render_breadcrumb(r, f, x, y, w);
    y += 28;

    render_file_list(r, f, x, y, w, cr.h - 64);

    SDL_RenderSetClipRect(r, nullptr);
}

// ── Toolbar ─────────────────────────────────────────────────────

void FileManagerApp::render_toolbar(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    // Back button
    SDL_Rect back = {x, y + 6, 60, 24};
    draw::filled_rounded_rect(r, back, 6, {40, 45, 70, 200});
    draw::text_centered(r, f->small, "< Back", x + 30, y + 10, WHITE);

    // Path display
    draw::text(r, f->title, "Files", x + 80, y + 8, WHITE);

    // Item count
    char buf[32];
    snprintf(buf, sizeof(buf), "%d items", (int)entries_.size());
    draw::text_right(r, f->small, buf, x + w, y + 12, DIM);

    draw::line(r, x, y + 34, x + w, y + 34, FAINT);
}

// ── Breadcrumb ──────────────────────────────────────────────────

void FileManagerApp::render_breadcrumb(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    (void)w;

    draw::icon(r, Icon::Box, x + 8, y + 10, 12, ACCENT);

    // Split path into segments
    int tx = x + 24;
    std::string path = current_path_;
    if (path.empty() || path == "/") {
        draw::text(r, f->body, "/  (root)", tx, y + 4, WHITE);
        return;
    }

    draw::text(r, f->body, "/", tx, y + 4, DIM);
    tx += 12;

    size_t pos = 1; // skip leading /
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        std::string seg = path.substr(pos, next - pos);
        if (!seg.empty()) {
            draw::text(r, f->body, seg.c_str(), tx, y + 4, WHITE);
            SDL_Point sz = draw::text_size(f->body, seg.c_str());
            tx += sz.x + 4;
            if (next != std::string::npos) {
                draw::text(r, f->body, "/", tx, y + 4, DIM);
                tx += 12;
            }
        }
        if (next == std::string::npos) break;
        pos = next + 1;
    }
}

// ── File list ───────────────────────────────────────────────────

void FileManagerApp::render_file_list(SDL_Renderer* r, const Fonts* f,
                                       int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    // Column headers
    int hy = y + 8;
    draw::text(r, f->small, "Name", x + PAD + 24, hy, DIM);
    draw::text(r, f->small, "Size", x + w - 200, hy, DIM);
    draw::text(r, f->small, "Modified", x + w - 120, hy, DIM);
    draw::line(r, x + PAD, hy + 16, x + w - PAD, hy + 16, FAINT);

    if (!ctx_.fs) {
        draw::text_centered(r, f->body, "No filesystem available",
                            x + w / 2, y + h / 2, DIM);
        content_h_ = h;
        return;
    }

    if (entries_.empty()) {
        draw::text_centered(r, f->body, "Empty directory",
                            x + w / 2, y + h / 2, DIM);
        content_h_ = h;
        return;
    }

    int iy = hy + 22 - (int)scroll_y_;
    for (int i = 0; i < (int)entries_.size(); i++) {
        auto& fi = entries_[i];

        // Skip if out of view
        if (iy + ROW_H < y || iy > y + h) {
            iy += ROW_H;
            continue;
        }

        // Selection highlight
        if (i == selected_index_) {
            draw::filled_rounded_rect(r, {x + 4, iy - 2, w - 8, ROW_H}, 4, {100, 150, 255, 20});
        }

        // Icon
        Icon ic = icon_for_file(fi);
        SDL_Color ic_col = fi.is_directory ? FOLDER_COL : DIM;
        draw::icon(r, ic, x + PAD + 10, iy + ROW_H / 2, 14, ic_col);

        // Name
        draw::text(r, f->body, fi.name.c_str(), x + PAD + 24, iy + 6, WHITE);

        // Size
        if (!fi.is_directory) {
            std::string sz = format_size(fi.size);
            draw::text(r, f->small, sz.c_str(), x + w - 200, iy + 8, DIM);
        } else {
            draw::text(r, f->small, "--", x + w - 200, iy + 8, DIM);
        }

        // Modified
        std::string mod = format_time(fi.modified);
        draw::text(r, f->small, mod.c_str(), x + w - 120, iy + 8, DIM);

        iy += ROW_H;
    }

    content_h_ = (int)entries_.size() * ROW_H + 40;
}

// ── Input handlers ──────────────────────────────────────────────

void FileManagerApp::on_mouse_down(int local_x, int local_y) {
    (void)local_x;

    // Back button
    if (local_y >= 6 && local_y < 30 && local_x >= 0 && local_x < 60) {
        if (current_path_ != "/" && !current_path_.empty()) {
            // Go up one level
            size_t slash = current_path_.rfind('/', current_path_.size() - 2);
            if (slash == std::string::npos || slash == 0) {
                navigate("/");
            } else {
                navigate(current_path_.substr(0, slash + 1));
            }
        }
        return;
    }

    // File list clicks (below header area)
    int list_start = 64 + 22;
    if (local_y >= list_start) {
        int row = (local_y - list_start + (int)scroll_y_) / ROW_H;
        if (row >= 0 && row < (int)entries_.size()) {
            if (entries_[row].is_directory) {
                std::string path = current_path_;
                if (path.back() != '/') path += '/';
                path += entries_[row].name;
                navigate(path);
            } else {
                selected_index_ = row;
            }
        }
    }
}

void FileManagerApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x;
    (void)local_y;

    scroll_y_ -= scroll_y * 20;

    int visible_h = last_rect_.h - 64;
    int max_scroll = content_h_ - visible_h;
    if (max_scroll < 0) max_scroll = 0;

    if (scroll_y_ < 0) scroll_y_ = 0;
    if (scroll_y_ > max_scroll) scroll_y_ = max_scroll;
}
