#include "journal_app.h"
#include "../ui.h"

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};

// ── Render ──────────────────────────────────────────────────────

void JournalApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_content_rect_ = cr;
    SDL_Renderer* r = ctx.r;

    int jx = cr.x;
    int jy = cr.y;
    int jw = cr.w;
    int jh = cr.h;

    // Left nav panel
    SDL_Rect nav = {jx + 1, jy + 1, nav_w_, jh - 2};
    draw::filled_rounded_rect(r, nav, 0, {10, 12, 22, 60});
    draw::line(r, jx + nav_w_, jy + 1, jx + nav_w_, jy + jh, {180, 195, 220, 20});

    struct NavItem { const char* label; Icon icon; };
    NavItem items[] = {
        {"Entries",   Icon::Journal},
        {"Favorites", Icon::Star},
        {"Insights",  Icon::Sparkle},
        {"Templates", Icon::Grid},
        {"Archive",   Icon::Box},
    };

    int ny = jy + 10;
    for (int i = 0; i < nav_item_count_; i++) {
        bool active = (i == active_nav_);
        bool hovered = (i == hover_nav_) && !active;
        SDL_Color ic = active ? ACCENT : DIM;

        if (active) {
            SDL_Rect hi = {jx + 4, ny - 2, nav_w_ - 6, 22};
            draw::filled_rounded_rect(r, hi, 4, {100, 150, 255, 25});
            draw::filled_circle(r, jx + 8, ny + 9, 2, ACCENT);
        } else if (hovered) {
            SDL_Rect hi = {jx + 4, ny - 2, nav_w_ - 6, 22};
            draw::filled_rounded_rect(r, hi, 4, {100, 150, 255, 12});
        }

        draw::icon(r, items[i].icon, jx + 22, ny + 9, 12, ic);
        draw::text(r, ctx.fonts->small, items[i].label, jx + 34, ny + 2, active ? WHITE : DIM);
        ny += 26;
    }

    // Content area
    int content_x = jx + nav_w_ + 16;
    int content_y = jy + 16;

    draw::text(r, ctx.fonts->title, "Morning Reflection", content_x, content_y, WHITE);
    content_y += 28;

    const char* body_lines[] = {
        "Today, I choose patience and",
        "presence. Each moment offers",
        "its own wisdom, if I remain",
        "still enough to receive it.",
    };
    for (auto* line : body_lines) {
        draw::text(r, ctx.fonts->body, line, content_x, content_y, {180, 185, 200, 220});
        content_y += 18;
    }

    content_y += 8;
    draw::line(r, content_x, content_y, jx + jw - 16, content_y, {180, 195, 220, 25});
    content_y += 14;

    draw::text(r, ctx.fonts->body, "In stillness, understanding.",
               content_x, content_y, {160, 170, 195, 180});

    // Bottom toolbar
    int toolbar_y = jy + jh - 28;
    draw::line(r, jx + nav_w_, toolbar_y - 4, jx + jw, toolbar_y - 4, {180, 195, 220, 20});

    Icon tools[] = {Icon::Pen, Icon::Image, Icon::Pin, Icon::Check, Icon::Lock, Icon::Dots};
    int tx = content_x;
    for (int i = 0; i < tool_count_; i++) {
        bool hovered = (i == hover_tool_);
        SDL_Color tc = hovered ? ACCENT : DIM;
        draw::icon(r, tools[i], tx, toolbar_y + 6, 12, tc);
        tx += 28;
    }
}

// ── Mouse interaction ───────────────────────────────────────────

void JournalApp::on_mouse_down(int local_x, int local_y) {
    // Check nav items
    if (local_x < nav_w_) {
        int ny = 10;
        for (int i = 0; i < nav_item_count_; i++) {
            if (local_y >= ny - 2 && local_y < ny + 22) {
                active_nav_ = i;
                return;
            }
            ny += 26;
        }
    }
}

void JournalApp::on_mouse_move(int local_x, int local_y) {
    hover_nav_ = -1;
    hover_tool_ = -1;

    // Check nav items
    if (local_x < nav_w_) {
        int ny = 10;
        for (int i = 0; i < nav_item_count_; i++) {
            if (local_y >= ny - 2 && local_y < ny + 22) {
                hover_nav_ = i;
                return;
            }
            ny += 26;
        }
    }

    // Check toolbar icons
    int toolbar_y = last_content_rect_.h - 28;
    int tx = nav_w_ + 16;
    if (local_y >= toolbar_y - 4 && local_y <= toolbar_y + 18) {
        for (int i = 0; i < tool_count_; i++) {
            if (local_x >= tx - 8 && local_x < tx + 20) {
                hover_tool_ = i;
                return;
            }
            tx += 28;
        }
    }
}
