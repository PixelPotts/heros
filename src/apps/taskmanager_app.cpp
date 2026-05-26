#include "taskmanager_app.h"
#include "../heros_sdk.h"
#include "../ui.h"
#include "../process.h"
#include <cstdio>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <sched.h>
#include <unistd.h>

// ── Constants ───────────────────────────────────────────────────

static const int PAD = 16;
static const int GAP = 12;
static const int ROW_H = 32;

// ── Colors ──────────────────────────────────────────────────────

static const SDL_Color CARD_BG = {22, 27, 55, 200};
static const SDL_Color WHITE   = {230, 230, 240, 255};
static const SDL_Color DIM     = {150, 160, 180, 255};
static const SDL_Color FAINT   = {255, 255, 255, 20};
static const SDL_Color ACCENT  = {100, 150, 255, 255};
static const SDL_Color GREEN   = {80, 200, 120, 255};
static const SDL_Color RED     = {220, 80, 80, 255};
static const SDL_Color YELLOW  = {230, 200, 60, 255};

// ── Helpers ─────────────────────────────────────────────────────

static void card_bg(SDL_Renderer* r, int x, int y, int w, int h) {
    draw::filled_rounded_rect(r, {x, y, w, h}, 8, CARD_BG);
}

static SDL_Color state_color(ProcessState s) {
    switch (s) {
        case ProcessState::Running:     return GREEN;
        case ProcessState::Starting:    return YELLOW;
        case ProcessState::Suspended:   return YELLOW;
        case ProcessState::Terminating: return RED;
        case ProcessState::Dead:        return DIM;
    }
    return DIM;
}

// ── Main render ─────────────────────────────────────────────────

void TaskManagerApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_rect_ = cr;
    SDL_Renderer* r = ctx.r;
    const Fonts* f = ctx.fonts;

    SDL_RenderSetClipRect(r, &cr);

    int x = cr.x + PAD;
    int w = cr.w - PAD * 2;
    int y = cr.y + PAD - (int)scroll_y_;

    render_header(r, f, x, y, w);
    y += 54;

    if (tab_ == Tab::Processes) {
        render_process_list(r, f, x, y, w, cr.h - 70);
    } else {
        render_system_stats(r, f, x, y, w, cr.h - 70);
    }

    SDL_RenderSetClipRect(r, nullptr);
}

// ── Header ──────────────────────────────────────────────────────

void TaskManagerApp::render_header(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    draw::text(r, f->widget, "Task Manager", x, y, WHITE);

    // Tab buttons
    int tx = x + w - 200;
    const char* tabs[] = {"Processes", "System"};
    Tab tab_vals[] = {Tab::Processes, Tab::System};

    for (int i = 0; i < 2; i++) {
        SDL_Rect btn = {tx, y + 6, 90, 26};
        if (tab_ == tab_vals[i]) {
            draw::filled_rounded_rect(r, btn, 6, {100, 150, 255, 40});
            draw::text_centered(r, f->body, tabs[i], tx + 45, y + 10, WHITE);
        } else {
            draw::rounded_rect(r, btn, 6, FAINT);
            draw::text_centered(r, f->body, tabs[i], tx + 45, y + 10, DIM);
        }
        tx += 100;
    }

    // Process count
    int count = 0;
    if (ctx_.pm) count = ctx_.pm->process_count();
    char buf[32];
    snprintf(buf, sizeof(buf), "%d processes", count);
    draw::text(r, f->small, buf, x, y + 34, DIM);
}

// ── Process list ────────────────────────────────────────────────

void TaskManagerApp::render_process_list(SDL_Renderer* r, const Fonts* f,
                                          int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    // Column headers
    int hy = y + PAD;
    draw::text(r, f->small, "PID", x + PAD, hy, DIM);
    draw::text(r, f->small, "App ID", x + 60, hy, DIM);
    draw::text(r, f->small, "State", x + w - 180, hy, DIM);
    draw::text(r, f->small, "Window", x + w - 100, hy, DIM);
    draw::line(r, x + PAD, hy + 18, x + w - PAD, hy + 18, FAINT);

    int iy = hy + 24;

    if (!ctx_.pm) {
        draw::text_centered(r, f->body, "No process manager", x + w / 2, iy + 20, DIM);
        content_h_ = 120;
        return;
    }

    auto procs = ctx_.pm->list_processes();

    for (auto* pi : procs) {
        bool selected = ((int)pi->pid == selected_pid_);
        if (selected) {
            draw::filled_rounded_rect(r, {x + 4, iy - 2, w - 8, ROW_H}, 4, {100, 150, 255, 20});
        }

        // PID
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%u", pi->pid);
        draw::text(r, f->body, pid_str, x + PAD, iy + 6, WHITE);

        // App ID (truncate if too long)
        const char* app_str = pi->app_id.c_str();
        draw::text(r, f->body, app_str, x + 60, iy + 6, WHITE);

        // State
        const char* state_str = process_state_str(pi->state);
        draw::text(r, f->small, state_str, x + w - 180, iy + 8, state_color(pi->state));

        // Window ID
        if (pi->window_id >= 0) {
            char win_str[16];
            snprintf(win_str, sizeof(win_str), "#%d", pi->window_id);
            draw::text(r, f->small, win_str, x + w - 100, iy + 8, DIM);
        } else {
            draw::text(r, f->small, "service", x + w - 100, iy + 8, DIM);
        }

        iy += ROW_H;
    }

    content_h_ = (int)procs.size() * ROW_H + 80;
}

// ── Resource isolation reading ──────────────────────────────────

static std::string format_cpu_set(cpu_set_t* set, int max_cpus) {
    std::string result;
    int range_start = -1;
    for (int i = 0; i <= max_cpus; i++) {
        bool in_set = (i < max_cpus) && CPU_ISSET(i, set);
        if (in_set && range_start < 0) {
            range_start = i;
        } else if (!in_set && range_start >= 0) {
            if (!result.empty()) result += ",";
            if (i - 1 == range_start)
                result += std::to_string(range_start);
            else
                result += std::to_string(range_start) + "-" + std::to_string(i - 1);
            range_start = -1;
        }
    }
    return result;
}

static std::string read_file_line(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) return {};
    char buf[256];
    buf[0] = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        // strip newline
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = 0;
    }
    fclose(fp);
    return buf;
}

void TaskManagerApp::read_resources() {
    time_t now = time(nullptr);
    if (now - res_last_update_ < 2) return;
    res_last_update_ = now;

    // CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        res_.total_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
        res_.cpu_count = CPU_COUNT(&cpuset);
        res_.allowed_cpus = format_cpu_set(&cpuset, res_.total_cpus);
    }

    // Find our cgroup path
    std::string cgroup_path;
    FILE* fp = fopen("/proc/self/cgroup", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            // Format: "0::/path"  (cgroup v2)
            if (strncmp(line, "0::", 3) == 0) {
                char* p = line + 3;
                size_t len = strlen(p);
                if (len > 0 && p[len - 1] == '\n') p[len - 1] = 0;
                cgroup_path = std::string("/sys/fs/cgroup") + p;
                break;
            }
        }
        fclose(fp);
    }

    // Memory limit
    res_.mem_limit_bytes = 0;
    if (!cgroup_path.empty()) {
        std::string val = read_file_line((cgroup_path + "/memory.max").c_str());
        if (!val.empty() && val != "max") {
            res_.mem_limit_bytes = strtoull(val.c_str(), nullptr, 10);
        }
    }

    // Memory current usage
    res_.mem_used_bytes = 0;
    if (!cgroup_path.empty()) {
        std::string val = read_file_line((cgroup_path + "/memory.current").c_str());
        if (!val.empty()) {
            res_.mem_used_bytes = strtoull(val.c_str(), nullptr, 10);
        }
    }

    res_.isolated = (res_.cpu_count > 0 && res_.cpu_count < res_.total_cpus);
}

static std::string format_bytes(uint64_t bytes) {
    char buf[32];
    if (bytes >= (uint64_t)1073741824)
        snprintf(buf, sizeof(buf), "%.1f GB", (double)bytes / 1073741824.0);
    else if (bytes >= 1048576)
        snprintf(buf, sizeof(buf), "%llu MB", (unsigned long long)(bytes / 1048576));
    else
        snprintf(buf, sizeof(buf), "%llu KB", (unsigned long long)(bytes / 1024));
    return buf;
}

// ── System stats ────────────────────────────────────────────────

void TaskManagerApp::render_system_stats(SDL_Renderer* r, const Fonts* f,
                                          int x, int y, int w, int h) {
    // Uptime card
    card_bg(r, x, y, w, 100);
    draw::text(r, f->title, "System Information", x + PAD, y + PAD, WHITE);

    time_t uptime = 0;
    if (ctx_.pm && ctx_.pm->start_time() > 0) {
        uptime = time(nullptr) - ctx_.pm->start_time();
    }
    int hours = (int)(uptime / 3600);
    int mins = (int)((uptime % 3600) / 60);
    int secs = (int)(uptime % 60);

    char uptime_str[64];
    snprintf(uptime_str, sizeof(uptime_str), "%dh %dm %ds", hours, mins, secs);

    int sy = y + 44;
    draw::text(r, f->body, "Uptime", x + PAD, sy, DIM);
    draw::text_right(r, f->body, uptime_str, x + w - PAD, sy, WHITE);
    sy += 24;
    draw::text(r, f->body, "Renderer", x + PAD, sy, DIM);
    draw::text_right(r, f->body, "SDL2 Accelerated", x + w - PAD, sy, WHITE);

    // Resource Isolation card
    read_resources();
    int ri_h = 140;
    int ry = y + 100 + GAP;
    card_bg(r, x, ry, w, ri_h);
    draw::text(r, f->title, "Resource Isolation", x + PAD, ry + PAD, WHITE);

    int iy_r = ry + 44;

    // CPU cores
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s (%d cores)", res_.allowed_cpus.c_str(), res_.cpu_count);
        draw::text(r, f->body, "CPU Cores", x + PAD, iy_r, DIM);
        draw::text_right(r, f->body, buf, x + w - PAD, iy_r, WHITE);
        iy_r += 24;
    }

    // Memory usage + progress bar
    if (res_.mem_limit_bytes > 0) {
        std::string used_str = format_bytes(res_.mem_used_bytes) + " / " + format_bytes(res_.mem_limit_bytes);
        draw::text(r, f->body, "Memory", x + PAD, iy_r, DIM);
        draw::text_right(r, f->body, used_str.c_str(), x + w - PAD, iy_r, WHITE);
        iy_r += 22;

        // Progress bar
        int bar_x = x + PAD;
        int bar_w = w - PAD * 2;
        int bar_h = 8;
        draw::filled_rounded_rect(r, {bar_x, iy_r, bar_w, bar_h}, 4, {255, 255, 255, 20});
        double frac = (double)res_.mem_used_bytes / (double)res_.mem_limit_bytes;
        if (frac > 1.0) frac = 1.0;
        int fill_w = (int)(bar_w * frac);
        if (fill_w > 0) {
            SDL_Color bar_col = (frac < 0.75) ? GREEN : (frac < 0.9 ? YELLOW : RED);
            draw::filled_rounded_rect(r, {bar_x, iy_r, fill_w, bar_h}, 4, bar_col);
        }
        iy_r += 16;
    } else {
        draw::text(r, f->body, "Memory Limit", x + PAD, iy_r, DIM);
        draw::text_right(r, f->body, "None (unlimited)", x + w - PAD, iy_r, DIM);
        iy_r += 24;
    }

    // Status dot + label
    {
        SDL_Color dot_col = res_.isolated ? GREEN : DIM;
        const char* label = res_.isolated ? "Isolated" : "No isolation";
        SDL_Color label_col = res_.isolated ? GREEN : DIM;
        // dot
        SDL_Rect dot = {x + PAD, iy_r + 4, 8, 8};
        draw::filled_rounded_rect(r, dot, 4, dot_col);
        draw::text(r, f->body, label, x + PAD + 14, iy_r, label_col);
    }

    // Process breakdown
    int cy = ry + ri_h + GAP;
    int remaining_h = h - (100 + GAP + ri_h + GAP);
    if (remaining_h < 60) remaining_h = 60;
    card_bg(r, x, cy, w, remaining_h);
    draw::text(r, f->title, "Process Breakdown", x + PAD, cy + PAD, WHITE);

    if (ctx_.pm) {
        auto procs = ctx_.pm->list_processes();
        int running = 0, services = 0, total = (int)procs.size();
        for (auto* pi : procs) {
            if (pi->state == ProcessState::Running) running++;
            if (pi->is_service) services++;
        }

        int iy = cy + 44;
        char buf[64];

        snprintf(buf, sizeof(buf), "%d", total);
        draw::text(r, f->body, "Total Processes", x + PAD, iy, DIM);
        draw::text_right(r, f->body, buf, x + w - PAD, iy, WHITE);
        iy += 24;

        snprintf(buf, sizeof(buf), "%d", running);
        draw::text(r, f->body, "Running", x + PAD, iy, DIM);
        draw::text_right(r, f->body, buf, x + w - PAD, iy, GREEN);
        iy += 24;

        snprintf(buf, sizeof(buf), "%d", services);
        draw::text(r, f->body, "Services", x + PAD, iy, DIM);
        draw::text_right(r, f->body, buf, x + w - PAD, iy, ACCENT);
        iy += 24;

        snprintf(buf, sizeof(buf), "%d", total - services);
        draw::text(r, f->body, "Windowed Apps", x + PAD, iy, DIM);
        draw::text_right(r, f->body, buf, x + w - PAD, iy, WHITE);
    }

    content_h_ = h;
}

// ── Input handlers ──────────────────────────────────────────────

void TaskManagerApp::on_mouse_down(int local_x, int local_y) {
    int w = last_rect_.w - PAD * 2;

    // Tab switch
    int tx = w - 200;
    if (local_y >= 6 && local_y < 32) {
        if (local_x >= tx && local_x < tx + 90) {
            tab_ = Tab::Processes;
            scroll_y_ = 0;
            return;
        }
        if (local_x >= tx + 100 && local_x < tx + 190) {
            tab_ = Tab::System;
            scroll_y_ = 0;
            return;
        }
    }

    // Process selection
    if (tab_ == Tab::Processes && local_y >= 78) {
        int row = (local_y - 78 + (int)scroll_y_) / ROW_H;
        if (ctx_.pm) {
            auto procs = ctx_.pm->list_processes();
            if (row >= 0 && row < (int)procs.size()) {
                selected_pid_ = (int)procs[row]->pid;
            }
        }
    }
}

void TaskManagerApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x;
    (void)local_y;

    scroll_y_ -= scroll_y * 20;

    int visible_h = last_rect_.h;
    int max_scroll = content_h_ - visible_h;
    if (max_scroll < 0) max_scroll = 0;

    if (scroll_y_ < 0) scroll_y_ = 0;
    if (scroll_y_ > max_scroll) scroll_y_ = max_scroll;
}

HEROS_APP(TaskManagerApp, "com.heros.taskmanager", "Task Manager", "0.1.0")
