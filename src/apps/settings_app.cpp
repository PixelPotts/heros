#include "settings_app.h"
#include "../heros_sdk.h"
#include "../ui.h"
#include "../vfs.h"
#include "../event_bus.h"
#include <cstdio>
#include <algorithm>

// ── Constants ───────────────────────────────────────────────────

static const int PAD = 16;
static const int GAP = 12;
static const int SIDE_W = 160;
static const int ROW_H = 36;

// ── Colors ──────────────────────────────────────────────────────

static const SDL_Color CARD_BG = {22, 27, 55, 200};
static const SDL_Color WHITE   = {230, 230, 240, 255};
static const SDL_Color DIM     = {150, 160, 180, 255};
static const SDL_Color FAINT   = {255, 255, 255, 20};
static const SDL_Color ACCENT  = {100, 150, 255, 255};
static const SDL_Color GREEN   = {80, 200, 120, 255};

// ── Helpers ─────────────────────────────────────────────────────

static void card_bg(SDL_Renderer* r, int x, int y, int w, int h) {
    draw::filled_rounded_rect(r, {x, y, w, h}, 8, CARD_BG);
}

static void toggle_switch(SDL_Renderer* r, int x, int y, bool on) {
    SDL_Color track = on ? SDL_Color{60, 130, 220, 200} : SDL_Color{50, 55, 75, 200};
    draw::filled_rounded_rect(r, {x, y, 36, 18}, 9, track);
    int knob_x = on ? x + 20 : x + 4;
    draw::filled_circle(r, knob_x + 5, y + 9, 6, WHITE);
}

static void setting_row(SDL_Renderer* r, const Fonts* f, int x, int y, int w,
                         const char* label, const char* value) {
    draw::text(r, f->body, label, x + PAD, y + 10, WHITE);
    draw::text_right(r, f->body, value, x + w - PAD, y + 10, DIM);
    draw::line(r, x + PAD, y + ROW_H - 1, x + w - PAD, y + ROW_H - 1, FAINT);
}

static void setting_toggle(SDL_Renderer* r, const Fonts* f, int x, int y, int w,
                            const char* label, bool on) {
    draw::text(r, f->body, label, x + PAD, y + 10, WHITE);
    toggle_switch(r, x + w - PAD - 36, y + 10, on);
    draw::line(r, x + PAD, y + ROW_H - 1, x + w - PAD, y + ROW_H - 1, FAINT);
}

// ── Main render ─────────────────────────────────────────────────

void SettingsApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_rect_ = cr;
    SDL_Renderer* r = ctx.r;
    const Fonts* f = ctx.fonts;

    SDL_RenderSetClipRect(r, &cr);

    // Left sidebar
    render_sidebar(r, f, cr.x, cr.y, SIDE_W, cr.h);

    // Content area
    int cx = cr.x + SIDE_W + GAP;
    int cw = cr.w - SIDE_W - GAP - PAD;
    int cy = cr.y + PAD - (int)scroll_y_;

    switch (page_) {
        case Page::General:       render_general(r, f, cx, cy, cw); break;
        case Page::Display:       render_display(r, f, cx, cy, cw); break;
        case Page::Notifications: render_notifications(r, f, cx, cy, cw); break;
        case Page::About:         render_about(r, f, cx, cy, cw); break;
    }

    SDL_RenderSetClipRect(r, nullptr);
}

// ── Sidebar ─────────────────────────────────────────────────────

void SettingsApp::render_sidebar(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    struct NavItem { const char* label; Icon icon; Page page; };
    NavItem items[] = {
        {"General",       Icon::Gear,    Page::General},
        {"Display",       Icon::Image,   Page::Display},
        {"Notifications", Icon::Bell,    Page::Notifications},
        {"About",         Icon::Sparkle, Page::About},
    };

    int iy = y + PAD;
    draw::text(r, f->title, "Settings", x + PAD, iy, WHITE);
    iy += 30;

    for (int i = 0; i < 4; i++) {
        SDL_Rect item = {x + 6, iy, w - 12, 30};
        bool active = (items[i].page == page_);

        if (active) {
            draw::filled_rounded_rect(r, item, 6, {100, 150, 255, 30});
            draw::icon(r, items[i].icon, x + 22, iy + 15, 14, ACCENT);
            draw::text(r, f->body, items[i].label, x + 38, iy + 8, WHITE);
        } else {
            draw::icon(r, items[i].icon, x + 22, iy + 15, 14, DIM);
            draw::text(r, f->body, items[i].label, x + 38, iy + 8, DIM);
        }
        iy += 32;
    }
}

// ── General page ────────────────────────────────────────────────

void SettingsApp::render_general(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    draw::text(r, f->widget, "General", x, y, WHITE);
    draw::text(r, f->small, "System-wide preferences", x, y + 28, DIM);
    y += 50;

    card_bg(r, x, y, w, 220);

    draw::text(r, f->title, "User Profile", x + PAD, y + PAD, WHITE);
    y += 40;

    setting_row(r, f, x, y, w, "Username", "hero");
    y += ROW_H;
    setting_row(r, f, x, y, w, "Home Directory", "~/.heros/user/");
    y += ROW_H;
    setting_row(r, f, x, y, w, "Language", "English");
    y += ROW_H;
    setting_row(r, f, x, y, w, "Timezone", "UTC");
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Autostart Finance", true);
    y += ROW_H + GAP;

    card_bg(r, x, y, w, 150);
    draw::text(r, f->title, "Session", x + PAD, y + PAD, WHITE);
    y += 40;

    setting_toggle(r, f, x, y, w, "Restore windows on boot", true);
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Auto-save on close", true);
    y += ROW_H;
    setting_row(r, f, x, y, w, "Session file", "/system/session.dat");

    content_h_ = 460;
}

// ── Display page ────────────────────────────────────────────────

void SettingsApp::render_display(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    draw::text(r, f->widget, "Display", x, y, WHITE);
    draw::text(r, f->small, "Visual appearance settings", x, y + 28, DIM);
    y += 50;

    card_bg(r, x, y, w, 220);
    draw::text(r, f->title, "Appearance", x + PAD, y + PAD, WHITE);
    y += 40;

    setting_row(r, f, x, y, w, "Theme", "Dark");
    y += ROW_H;
    setting_row(r, f, x, y, w, "Accent Color", "Blue");
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Frosted Glass Effects", true);
    y += ROW_H;
    setting_row(r, f, x, y, w, "Font Size", "Normal");
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Window Animations", true);
    y += ROW_H + GAP;

    card_bg(r, x, y, w, 110);
    draw::text(r, f->title, "Desktop", x + PAD, y + PAD, WHITE);
    y += 40;

    setting_row(r, f, x, y, w, "Wallpaper", "Unsplash Forest");
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Show geometric overlay", true);

    content_h_ = 420;
}

// ── Notifications page ──────────────────────────────────────────

void SettingsApp::render_notifications(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    draw::text(r, f->widget, "Notifications", x, y, WHITE);
    draw::text(r, f->small, "Control how you receive alerts", x, y + 28, DIM);
    y += 50;

    card_bg(r, x, y, w, 185);
    draw::text(r, f->title, "Toast Notifications", x + PAD, y + PAD, WHITE);
    y += 40;

    setting_toggle(r, f, x, y, w, "Enable notifications", true);
    y += ROW_H;
    setting_row(r, f, x, y, w, "Display duration", "4 seconds");
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Sound on notification", false);
    y += ROW_H;
    setting_toggle(r, f, x, y, w, "Quiet Hours auto-enable", true);

    content_h_ = 280;
}

// ── About page ──────────────────────────────────────────────────

void SettingsApp::render_about(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    draw::text(r, f->widget, "About", x, y, WHITE);
    y += 40;

    card_bg(r, x, y, w, 250);

    int cy = y + PAD;
    draw::icon(r, Icon::Ring, x + w / 2, cy + 18, 28, ACCENT);
    draw::glow(r, x + w / 2, cy + 18, 16, {100, 150, 255, 40});
    cy += 40;

    draw::text_centered(r, f->widget, "HerOS", x + w / 2, cy, WHITE);
    cy += 30;
    draw::text_centered(r, f->body, "Version 0.1.0 (Sol 1.618)", x + w / 2, cy, DIM);
    cy += 24;
    draw::text_centered(r, f->small, "A contemplative desktop operating system", x + w / 2, cy, DIM);
    cy += 20;

    draw::line(r, x + PAD, cy, x + w - PAD, cy, FAINT);
    cy += 12;

    setting_row(r, f, x, cy - y + y, w, "Kernel", "SDL2 Virtual");
    cy += ROW_H;
    setting_row(r, f, x, cy - y + y, w, "Architecture", "x86_64");
    cy += ROW_H;
    setting_row(r, f, x, cy - y + y, w, "Built with", "C++17 / SDL2");

    content_h_ = 340;
}

// ── Input handlers ──────────────────────────────────────────────

void SettingsApp::on_mouse_down(int local_x, int local_y) {
    // Check sidebar nav clicks
    if (local_x < SIDE_W) {
        int iy = PAD + 30;  // after "Settings" title
        Page pages[] = {Page::General, Page::Display, Page::Notifications, Page::About};
        for (int i = 0; i < 4; i++) {
            if (local_y >= iy && local_y < iy + 30) {
                page_ = pages[i];
                scroll_y_ = 0;
                return;
            }
            iy += 32;
        }
    }
}

void SettingsApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x;
    (void)local_y;

    scroll_y_ -= scroll_y * 20;

    int visible_h = last_rect_.h;
    int max_scroll = content_h_ - visible_h;
    if (max_scroll < 0) max_scroll = 0;

    if (scroll_y_ < 0) scroll_y_ = 0;
    if (scroll_y_ > max_scroll) scroll_y_ = max_scroll;
}

HEROS_APP(SettingsApp, "com.heros.settings", "Settings", "0.1.0")
