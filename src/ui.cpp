#include "ui.h"
#include "app_registry.h"
#include <cmath>
#include <algorithm>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};
static const SDL_Color FAINT  = {255, 255, 255, 20};

// ── Background ──────────────────────────────────────────────────

void render_background(SDL_Renderer* r, SDL_Texture* bg, int w, int h) {
    if (bg) {
        int tw, th;
        SDL_QueryTexture(bg, nullptr, nullptr, &tw, &th);
        float scale = std::max((float)w / tw, (float)h / th);
        int dw = (int)(tw * scale), dh = (int)(th * scale);
        SDL_Rect dst = {(w - dw) / 2, (h - dh) / 2, dw, dh};
        SDL_RenderCopy(r, bg, nullptr, &dst);
    } else {
        for (int y = 0; y < h; y++) {
            float t = (float)y / h;
            SDL_SetRenderDrawColor(r, (Uint8)(10 + t * 15), (Uint8)(15 + t * 25),
                                      (Uint8)(40 + t * 50), 255);
            SDL_RenderDrawLine(r, 0, y, w, y);
        }
    }
    // Slight dark overlay for contrast
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 20, 50);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
}

// ── Geometric Overlay ───────────────────────────────────────────

void render_geometric_overlay(SDL_Renderer* r, int w, int h) {
    int cx = w * 42 / 100;
    int cy = h * 38 / 100;

    // Concentric circles
    int radii[] = {60, 120, 200, 300};
    for (int rad : radii)
        draw::circle(r, cx, cy, rad, {255, 255, 255, 18});

    // Dotted circles
    draw::dotted_circle(r, cx, cy, 90, 60, {255, 255, 255, 30});
    draw::dotted_circle(r, cx, cy, 160, 90, {255, 255, 255, 22});
    draw::dotted_circle(r, cx, cy, 250, 120, {255, 255, 255, 15});

    // Vertical luminous line
    draw::line(r, cx, 0, cx, h, {255, 255, 255, 12});

    // Radial lines
    for (int i = 0; i < 8; i++) {
        float a = (float)M_PI * 2 * i / 8;
        int ex = cx + (int)(300 * cosf(a));
        int ey = cy + (int)(300 * sinf(a));
        draw::line(r, cx, cy, ex, ey, {255, 255, 255, 8});
    }

    // Glow nodes
    draw::glow(r, cx, cy, 10, {200, 220, 255, 120});
    draw::glow(r, cx, cy - 120, 6, {200, 220, 255, 80});
    draw::glow(r, cx + 90, cy + 50, 5, {200, 220, 255, 60});
    draw::glow(r, cx - 160, cy, 5, {200, 220, 255, 60});
    draw::glow(r, cx, cy + 200, 5, {200, 220, 255, 50});
    draw::glow(r, cx + 200, cy - 80, 4, {200, 220, 255, 50});
}

// ── Top Bar ─────────────────────────────────────────────────────

void render_topbar(const RenderCtx& ctx) {
    SDL_Rect bar = {0, 0, ctx.w, 36};
    ctx.frost->render_panel(ctx.r, bar, {10, 12, 25, 160});

    // Subtle bottom border
    draw::line(ctx.r, 0, 35, ctx.w, 35, {180, 195, 220, 30});

    // Left: logo ring + "H E R"
    draw::icon(ctx.r, Icon::Ring, 24, 18, 18, {200, 210, 240, 200});
    draw::text_spaced(ctx.r, ctx.fonts->brand, "HER", 38, 10, {200, 210, 240, 220}, 4);

    // Center: status
    draw::text_centered(ctx.r, ctx.fonts->body, "Sol 1.618 \xC2\xB7 Dawn",
                        ctx.w / 2, 10, DIM);

    // Right: system tray icons are now rendered by SystemTray
    // (see systray.cpp — called from main.cpp after render_topbar)
}

// ── Right Sidebar ───────────────────────────────────────────────

void render_right_sidebar(const RenderCtx& ctx) {
    int sx = ctx.w - 228, sy = 44;
    int sw = 220;

    // Card 1: Dawn
    {
        int ch = 155;
        SDL_Rect card = {sx, sy, sw, ch};
        ctx.frost->render_panel(ctx.r, card, {12, 15, 28, 150});
        draw::rounded_rect(ctx.r, card, 10, {180, 195, 220, 30});

        draw::text(ctx.r, ctx.fonts->title, "Dawn", sx + 14, sy + 10, WHITE);
        draw::text_right(ctx.r, ctx.fonts->small, "Sol 1.618", sx + sw - 14, sy + 12, DIM);

        // Dotted ring illustration
        int rcx = sx + sw / 2, rcy = sy + 70;
        draw::dotted_circle(ctx.r, rcx, rcy, 25, 30, {200, 210, 240, 80});
        draw::glow(ctx.r, rcx, rcy, 8, {255, 200, 120, 100});
        draw::filled_circle(ctx.r, rcx, rcy, 3, {255, 220, 150, 200});

        draw::text_centered(ctx.r, ctx.fonts->small, "Today", sx + sw / 2, sy + 105, DIM);
        draw::text_centered(ctx.r, ctx.fonts->body, "Focus \xC2\xB7 Create \xC2\xB7 Serve",
                            sx + sw / 2, sy + 125, {180, 190, 210, 200});
    }

    // Card 2: System Harmony
    {
        int cy = sy + 163;
        int ch = 125;
        SDL_Rect card = {sx, cy, sw, ch};
        ctx.frost->render_panel(ctx.r, card, {12, 15, 28, 150});
        draw::rounded_rect(ctx.r, card, 10, {180, 195, 220, 30});

        draw::text(ctx.r, ctx.fonts->title, "System Harmony", sx + 14, cy + 10, WHITE);

        // Gauge arc
        int gcx = sx + sw / 2, gcy = cy + 60;
        for (int i = 0; i < 50; i++) {
            float a = (float)M_PI * 0.8f + (float)M_PI * 1.4f * i / 50;
            int gx = gcx + (int)(30 * cosf(a));
            int gy = gcy + (int)(30 * sinf(a));
            SDL_Color gc = (i < 48) ? SDL_Color{180, 140, 80, 180} : SDL_Color{60, 60, 80, 80};
            draw::filled_circle(ctx.r, gx, gy, 1, gc);
        }
        draw::icon(ctx.r, Icon::Sparkle, gcx, gcy, 14, {180, 200, 240, 160});

        draw::text_centered(ctx.r, ctx.fonts->widget, "98%", gcx, cy + 85, WHITE);
        draw::text_centered(ctx.r, ctx.fonts->small, "Aligned", gcx, cy + 108, DIM);
    }

    // Card 3: Quiet Hours
    {
        int cy = sy + 296;
        int ch = 80;
        SDL_Rect card = {sx, cy, sw, ch};
        ctx.frost->render_panel(ctx.r, card, {12, 15, 28, 150});
        draw::rounded_rect(ctx.r, card, 10, {180, 195, 220, 30});

        draw::text(ctx.r, ctx.fonts->title, "Quiet Hours", sx + 14, cy + 10, WHITE);
        draw::text(ctx.r, ctx.fonts->body, "22:00 \xe2\x80\x94 06:00", sx + 14, cy + 32, DIM);
        draw::text_right(ctx.r, ctx.fonts->small, "Active", sx + sw - 14, cy + 34, {100, 200, 160, 200});

        draw::icon(ctx.r, Icon::Moon, sx + sw - 30, cy + 60, 18, {140, 160, 220, 180});
        draw::filled_circle(ctx.r, sx + sw - 20, cy + 52, 3, ACCENT);
    }

    // Card 4: Quote
    {
        int cy = sy + 384;
        int ch = 72;
        SDL_Rect card = {sx, cy, sw, ch};
        ctx.frost->render_panel(ctx.r, card, {12, 15, 28, 150});
        draw::rounded_rect(ctx.r, card, 10, {180, 195, 220, 30});

        draw::icon(ctx.r, Icon::Sparkle, sx + 18, cy + 14, 10, {200, 180, 130, 150});
        draw::text(ctx.r, ctx.fonts->small, "\"The quieter you become,", sx + 14, cy + 24, {170, 175, 195, 200});
        draw::text(ctx.r, ctx.fonts->small, "the more clearly you", sx + 14, cy + 38, {170, 175, 195, 200});
        draw::text(ctx.r, ctx.fonts->small, "can hear.\"", sx + 14, cy + 52, {170, 175, 195, 200});
    }
}

// ── Dock (registry-driven) ──────────────────────────────────────

// Build the list of dock entries: pinned apps + running non-pinned apps
struct DockEntry {
    std::string app_id;
    Icon icon;
    bool is_running;
    bool is_minimized;
    bool is_focused;
};

static std::vector<DockEntry> build_dock_entries(const WindowManager& wm,
                                                  const AppRegistry& registry) {
    std::vector<DockEntry> entries;

    // 1. Pinned apps (always shown, sorted by dock_order)
    auto pinned = registry.list_pinned_dock_apps();
    for (auto* m : pinned) {
        DockEntry e;
        e.app_id = m->app_id;
        e.icon = m->icon;
        e.is_running = registry.is_running(m->app_id);
        e.is_minimized = false;
        e.is_focused = false;

        // Check window state if running
        int wid = registry.find_window_for_app(m->app_id);
        if (wid >= 0) {
            auto* win = wm.find_window(wid);
            if (win) {
                e.is_minimized = win->minimized;
                e.is_focused = win->active;
            }
        }
        entries.push_back(e);
    }

    // 2. Running non-pinned apps (shown temporarily)
    for (auto& win : wm.windows()) {
        // Check if this window's app is already in pinned list
        bool already_in_dock = false;
        for (auto& de : entries) {
            int wid = registry.find_window_for_app(de.app_id);
            if (wid == win.id) { already_in_dock = true; break; }
        }
        if (!already_in_dock) {
            DockEntry e;
            e.icon = win.icon;
            e.is_running = true;
            e.is_minimized = win.minimized;
            e.is_focused = win.active;
            // Try to find app_id from registry
            for (auto* m : registry.list_apps()) {
                if (registry.find_window_for_app(m->app_id) == win.id) {
                    e.app_id = m->app_id;
                    break;
                }
            }
            entries.push_back(e);
        }
    }

    return entries;
}

static void get_dock_geometry(int screen_w, int screen_h, int num_icons,
                               int& dx, int& dy, int& dw, int& dh, int& spacing) {
    int min_dw = 120;
    int per_icon = 50;
    dw = std::max(min_dw, num_icons * per_icon + 24);
    dh = 48;
    dx = (screen_w - dw) / 2;
    dy = screen_h - 58;
    spacing = (num_icons > 0) ? (dw - 24) / num_icons : 0;
}

void render_dock(const RenderCtx& ctx, const WindowManager& wm,
                 const AppRegistry& registry) {
    auto entries = build_dock_entries(wm, registry);
    int num = (int)entries.size();
    if (num == 0) return;

    int dx, dy, dw, dh, spacing;
    get_dock_geometry(ctx.w, ctx.h, num, dx, dy, dw, dh, spacing);

    SDL_Rect dock = {dx, dy, dw, dh};
    ctx.frost->render_panel(ctx.r, dock, {12, 15, 28, 150});
    draw::rounded_rect(ctx.r, dock, 14, {180, 195, 220, 35});

    int ix = dx + 12 + spacing / 2;
    for (int i = 0; i < num; i++) {
        auto& e = entries[i];

        // Icon color: accent if running, dim otherwise
        SDL_Color ic = e.is_running ? ACCENT : SDL_Color{190, 200, 220, 200};
        draw::icon(ctx.r, e.icon, ix, dy + dh / 2, 20, ic);

        // Running indicator dot
        if (e.is_running && !e.is_minimized) {
            draw::filled_circle(ctx.r, ix, dy + dh - 5, 2, ACCENT);
        } else if (e.is_minimized) {
            draw::filled_circle(ctx.r, ix, dy + dh - 5, 2, DIM);
        }

        ix += spacing;
    }

    // Page dots
    int pcx = ctx.w / 2;
    for (int i = 0; i < 3; i++) {
        int px = pcx - 8 + i * 8;
        draw::filled_circle(ctx.r, px, dy + dh + 8, (i == 0) ? 2 : 1,
                            (i == 0) ? WHITE : DIM);
    }
}

// ── Dock hit detection ──────────────────────────────────────────

std::string dock_app_at(int mx, int my, int screen_w, int screen_h,
                        const WindowManager& wm, const AppRegistry& registry) {
    auto entries = build_dock_entries(wm, registry);
    int num = (int)entries.size();
    if (num == 0) return "";

    int dx, dy, dw, dh, spacing;
    get_dock_geometry(screen_w, screen_h, num, dx, dy, dw, dh, spacing);

    // Check if click is within dock bounds
    if (mx < dx || mx >= dx + dw || my < dy || my >= dy + dh)
        return "";

    int ix = dx + 12 + spacing / 2;
    for (int i = 0; i < num; i++) {
        if (mx >= ix - spacing / 2 && mx < ix + spacing / 2)
            return entries[i].app_id;
        ix += spacing;
    }
    return "";
}
