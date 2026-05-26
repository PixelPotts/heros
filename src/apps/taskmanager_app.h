#pragma once
#include "../window.h"
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

struct ResourceInfo {
    std::string allowed_cpus;   // e.g. "14-15"
    int         cpu_count = 0;
    int         total_cpus = 0;
    uint64_t    mem_limit_bytes = 0;  // 0 = no limit
    uint64_t    mem_used_bytes = 0;
    bool        isolated = false;
};

class TaskManagerApp : public AppContent {
public:
    void render(const RenderCtx& ctx, SDL_Rect content_rect) override;
    void on_mouse_down(int local_x, int local_y) override;
    void on_scroll(int local_x, int local_y, int scroll_y) override;

private:
    // ── Render sections ─────────────────────────────────────────
    void render_header(SDL_Renderer* r, const Fonts* f, int x, int y, int w);
    void render_process_list(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);
    void render_system_stats(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h);

    // ── Resource isolation ──────────────────────────────────────
    void read_resources();

    // ── State ───────────────────────────────────────────────────
    enum class Tab { Processes, System };
    Tab tab_ = Tab::Processes;
    int selected_pid_ = -1;
    float scroll_y_ = 0;
    int content_h_ = 0;
    SDL_Rect last_rect_ = {0, 0, 0, 0};

    // ── Resource cache ──────────────────────────────────────────
    ResourceInfo res_;
    time_t       res_last_update_ = 0;
};
