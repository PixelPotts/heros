#pragma once
#include "../window.h"
#include "../vfs.h"
#include <string>
#include <vector>

class FileManagerApp : public AppContent {
public:
    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_down(int local_x, int local_y) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;

private:
    void render_toolbar(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_breadcrumb(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_file_list(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);

    void navigate(const std::string& path);
    Icon icon_for_file(const FileInfo& fi) const;

    // ── State ───────────────────────────────────────────────────
    std::string current_path_ = "/";
    std::vector<FileInfo> entries_;
    bool needs_refresh_ = true;
    int selected_index_ = -1;
    float scroll_y_ = 0;
    int content_h_ = 0;
    SDL_Rect last_rect_ = {0, 0, 0, 0};
};
