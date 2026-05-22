#include "ui.h"
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

    // Right: system icons
    Icon icons[] = {Icon::Bell, Icon::Waveform, Icon::Grid, Icon::Volume, Icon::Power};
    int ix = ctx.w - 24;
    for (int i = 4; i >= 0; i--) {
        draw::icon(ctx.r, icons[i], ix, 18, 16, DIM);
        ix -= 28;
    }
}

// ── Left Sidebar ────────────────────────────────────────────────

void render_left_sidebar(const RenderCtx& ctx) {
    int sx = 8, sy = 44;
    int sw = 180, sh = ctx.h - 108;
    SDL_Rect panel = {sx, sy, sw, sh};
    ctx.frost->render_panel(ctx.r, panel, {12, 15, 28, 150});
    draw::rounded_rect(ctx.r, panel, 10, {180, 195, 220, 30});

    struct NavItem { const char* label; Icon icon; };
    NavItem items[] = {
        {"Sanctum",  Icon::Flower},
        {"Library",  Icon::Book},
        {"Journal",  Icon::Journal},
        {"Work",     Icon::Briefcase},
        {"Attune",   Icon::Sliders},
        {"Explore",  Icon::Compass},
        {"Communal", Icon::People},
        {"Settings", Icon::Gear},
    };

    int iy = sy + 12;
    for (int i = 0; i < 8; i++) {
        SDL_Rect item_rect = {sx + 6, iy, sw - 12, 30};
        if (i == 0) { // Active
            draw::filled_rounded_rect(ctx.r, item_rect, 6, {100, 150, 255, 30});
            draw::icon(ctx.r, items[i].icon, sx + 22, iy + 15, 16, ACCENT);
            draw::text(ctx.r, ctx.fonts->body, items[i].label, sx + 38, iy + 7, WHITE);
        } else {
            draw::icon(ctx.r, items[i].icon, sx + 22, iy + 15, 16, DIM);
            draw::text(ctx.r, ctx.fonts->body, items[i].label, sx + 38, iy + 7, DIM);
        }
        iy += 34;
    }

    // Bottom status widget
    int bh = 100;
    int by = sy + sh - bh - 8;
    SDL_Rect status_panel = {sx + 6, by, sw - 12, bh};
    draw::filled_rounded_rect(ctx.r, status_panel, 8, {20, 25, 40, 100});
    draw::rounded_rect(ctx.r, status_panel, 8, {180, 195, 220, 25});

    int scx = sx + sw / 2;
    draw::icon(ctx.r, Icon::Compass, scx, by + 25, 22, {100, 200, 160, 200});
    draw::glow(ctx.r, scx, by + 25, 14, {100, 200, 160, 40});
    draw::text_centered(ctx.r, ctx.fonts->body, "Aligned", scx, by + 42, WHITE);
    draw::text_centered(ctx.r, ctx.fonts->small, "All systems in harmony.", scx, by + 60, DIM);
    draw::text_centered(ctx.r, ctx.fonts->small, "Thank you.", scx, by + 76, {130, 140, 160, 180});
    draw::icon(ctx.r, Icon::ChevronUp, scx, by + bh - 6, 10, DIM);
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

// ── Dock ────────────────────────────────────────────────────────

void render_dock(const RenderCtx& ctx) {
    int dw = 380, dh = 48;
    int dx = (ctx.w - dw) / 2;
    int dy = ctx.h - 58;
    SDL_Rect dock = {dx, dy, dw, dh};
    ctx.frost->render_panel(ctx.r, dock, {12, 15, 28, 150});
    draw::rounded_rect(ctx.r, dock, 14, {180, 195, 220, 35});

    Icon dock_icons[] = {
        Icon::Flower, Icon::Target, Icon::Lotus, Icon::Journal,
        Icon::Ring, Icon::Mountain, Icon::Trash
    };
    int num = 7;
    int spacing = (dw - 24) / num;
    int ix = dx + 12 + spacing / 2;

    for (int i = 0; i < num; i++) {
        bool active = (i == 3); // Journal is active
        SDL_Color ic = active ? ACCENT : SDL_Color{190, 200, 220, 200};
        draw::icon(ctx.r, dock_icons[i], ix, dy + dh / 2, 20, ic);

        if (active)
            draw::filled_circle(ctx.r, ix, dy + dh - 5, 2, ACCENT);

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

// ── Journal Window ──────────────────────────────────────────────

void render_journal(const RenderCtx& ctx) {
    int jw = 500, jh = 380;
    int jx = (ctx.w - jw) / 2 - 20;
    int jy = 70;

    // Window shadow
    SDL_Rect shadow = {jx + 4, jy + 4, jw, jh};
    draw::filled_rounded_rect(ctx.r, shadow, 10, {0, 0, 0, 40});

    // Window body
    SDL_Rect win = {jx, jy, jw, jh};
    ctx.frost->render_panel(ctx.r, win, {10, 14, 25, 170});
    draw::rounded_rect(ctx.r, win, 10, {180, 195, 220, 40});

    // Title bar
    int tby = jy + 4;
    draw::line(ctx.r, jx, jy + 32, jx + jw, jy + 32, {180, 195, 220, 25});
    draw::text(ctx.r, ctx.fonts->body, "Journal \xe2\x80\x94 Morning Reflection",
               jx + 14, tby + 6, WHITE);

    // Window controls
    int cx = jx + jw - 16;
    draw::icon(ctx.r, Icon::Close, cx, tby + 14, 10, {200, 100, 100, 200});
    cx -= 20;
    draw::icon(ctx.r, Icon::Maximize, cx, tby + 14, 10, DIM);
    cx -= 20;
    draw::icon(ctx.r, Icon::Minimize, cx, tby + 14, 10, DIM);

    // Left nav panel
    int nav_w = 110;
    SDL_Rect nav = {jx + 1, jy + 33, nav_w, jh - 34};
    draw::filled_rounded_rect(ctx.r, nav, 0, {10, 12, 22, 60});
    draw::line(ctx.r, jx + nav_w, jy + 33, jx + nav_w, jy + jh, {180, 195, 220, 20});

    struct NavItem { const char* label; Icon icon; };
    NavItem items[] = {
        {"Entries",   Icon::Journal},
        {"Favorites", Icon::Star},
        {"Insights",  Icon::Sparkle},
        {"Templates", Icon::Grid},
        {"Archive",   Icon::Box},
    };

    int ny = jy + 42;
    for (int i = 0; i < 5; i++) {
        bool active = (i == 0);
        SDL_Color ic = active ? ACCENT : DIM;
        if (active) {
            SDL_Rect hi = {jx + 4, ny - 2, nav_w - 6, 22};
            draw::filled_rounded_rect(ctx.r, hi, 4, {100, 150, 255, 25});
            draw::filled_circle(ctx.r, jx + 8, ny + 9, 2, ACCENT);
        }
        draw::icon(ctx.r, items[i].icon, jx + 22, ny + 9, 12, ic);
        draw::text(ctx.r, ctx.fonts->small, items[i].label, jx + 34, ny + 2, active ? WHITE : DIM);
        ny += 26;
    }

    // Content area
    int content_x = jx + nav_w + 16;
    int content_y = jy + 48;

    draw::text(ctx.r, ctx.fonts->title, "Morning Reflection", content_x, content_y, WHITE);
    content_y += 28;

    const char* body_lines[] = {
        "Today, I choose patience and",
        "presence. Each moment offers",
        "its own wisdom, if I remain",
        "still enough to receive it.",
    };
    for (auto* line : body_lines) {
        draw::text(ctx.r, ctx.fonts->body, line, content_x, content_y, {180, 185, 200, 220});
        content_y += 18;
    }

    content_y += 8;
    draw::line(ctx.r, content_x, content_y, jx + jw - 16, content_y, {180, 195, 220, 25});
    content_y += 14;

    draw::text(ctx.r, ctx.fonts->body, "In stillness, understanding.",
               content_x, content_y, {160, 170, 195, 180});

    // Bottom toolbar
    int toolbar_y = jy + jh - 28;
    draw::line(ctx.r, jx + nav_w, toolbar_y - 4, jx + jw, toolbar_y - 4, {180, 195, 220, 20});

    Icon tools[] = {Icon::Pen, Icon::Image, Icon::Pin, Icon::Check, Icon::Lock, Icon::Dots};
    int tx = content_x;
    for (auto tool : tools) {
        draw::icon(ctx.r, tool, tx, toolbar_y + 6, 12, DIM);
        tx += 28;
    }
}
