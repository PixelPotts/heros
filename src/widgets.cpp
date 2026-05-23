#include "widgets.h"

namespace widget {

// ── Card ────────────────────────────────────────────────────────

void card(SDL_Renderer* r, const ThemeColors& tc, SDL_Rect rect, int radius) {
    draw::filled_rounded_rect(r, rect, radius, tc.bg_card);
}

// ── Button ──────────────────────────────────────────────────────

void button(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
            SDL_Rect rect, const char* label, ButtonStyle style) {
    SDL_Color bg, fg;

    switch (style) {
        case ButtonStyle::Primary:
            bg = tc.accent;
            fg = {255, 255, 255, 255};
            break;
        case ButtonStyle::Secondary:
            bg = tc.bg_card;
            fg = tc.text_primary;
            break;
        case ButtonStyle::Ghost:
            bg = {0, 0, 0, 0};
            fg = tc.accent;
            break;
        case ButtonStyle::Danger:
            bg = tc.error;
            fg = {255, 255, 255, 255};
            break;
    }

    if (style != ButtonStyle::Ghost) {
        draw::filled_rounded_rect(r, rect, 6, bg);
    }
    draw::rounded_rect(r, rect, 6, tc.border);
    draw::text_centered(r, f->body, label, rect.x + rect.w / 2, rect.y + rect.h / 2 - 7, fg);
}

// ── Toggle Switch ───────────────────────────────────────────────

void toggle(SDL_Renderer* r, const ThemeColors& tc, int x, int y, bool on) {
    SDL_Color track = on ? tc.accent : SDL_Color{50, 55, 75, 200};
    draw::filled_rounded_rect(r, {x, y, 36, 18}, 9, track);
    int knob_x = on ? x + 20 : x + 4;
    draw::filled_circle(r, knob_x + 5, y + 9, 6, tc.text_primary);
}

// ── Progress Bar ────────────────────────────────────────────────

void progress_bar(SDL_Renderer* r, const ThemeColors& tc, SDL_Rect rect,
                  float pct, SDL_Color fill_color) {
    SDL_Color track = {40, 45, 70, 200};
    draw::filled_rounded_rect(r, rect, rect.h / 2, track);

    int fw = (int)(rect.w * pct);
    if (fw > 0) {
        if (fw < rect.h) fw = rect.h;
        SDL_Color fc = (fill_color.a > 0) ? fill_color : tc.accent;
        draw::filled_rounded_rect(r, {rect.x, rect.y, fw, rect.h}, rect.h / 2, fc);
    }
}

// ── Badge / Pill ────────────────────────────────────────────────

void badge(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
           int x, int y, const char* text, SDL_Color bg_color) {
    SDL_Point sz = draw::text_size(f->small, text);
    int pw = sz.x + 12;
    int ph = sz.y + 4;
    SDL_Color bg = (bg_color.a > 0) ? bg_color : tc.accent_dim;
    draw::filled_rounded_rect(r, {x, y, pw, ph}, ph / 2, bg);
    draw::text(r, f->small, text, x + 6, y + 2, tc.text_primary);
}

// ── Separator ───────────────────────────────────────────────────

void separator(SDL_Renderer* r, const ThemeColors& tc, int x, int y, int w) {
    draw::line(r, x, y, x + w, y, tc.divider);
}

// ── Section Header ──────────────────────────────────────────────

void section_header(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
                    int x, int y, const char* title) {
    draw::text(r, f->title, title, x, y, tc.text_primary);
    draw::line(r, x, y + 20, x + 200, y + 20, tc.divider);
}

// ── Icon Button ─────────────────────────────────────────────────

void icon_button(SDL_Renderer* r, const ThemeColors& tc,
                 int cx, int cy, int size, Icon icon, bool active) {
    if (active) {
        draw::filled_circle(r, cx, cy, size + 4, tc.bg_selected);
    }
    draw::icon(r, icon, cx, cy, size, active ? tc.accent : tc.text_secondary);
}

// ── Tab Bar ─────────────────────────────────────────────────────

void tab_bar(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
             int x, int y, int w, const TabItem* tabs, int count) {
    int tab_w = w / count;
    for (int i = 0; i < count; i++) {
        int tx = x + i * tab_w;
        SDL_Rect btn = {tx + 2, y, tab_w - 4, 28};

        if (tabs[i].active) {
            draw::filled_rounded_rect(r, btn, 6, tc.bg_selected);
            draw::text_centered(r, f->body, tabs[i].label, tx + tab_w / 2, y + 6, tc.text_primary);
        } else {
            draw::text_centered(r, f->body, tabs[i].label, tx + tab_w / 2, y + 6, tc.text_secondary);
        }
    }

    // Bottom border
    draw::line(r, x, y + 29, x + w, y + 29, tc.divider);
}

// ── List Item ───────────────────────────────────────────────────

void list_item(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
               SDL_Rect rect, Icon icon, const char* title,
               const char* subtitle, bool selected) {
    if (selected) {
        draw::filled_rounded_rect(r, rect, 6, tc.bg_selected);
    }

    draw::icon(r, icon, rect.x + 16, rect.y + rect.h / 2, 14,
               selected ? tc.accent : tc.text_secondary);

    int text_y = subtitle ? rect.y + 6 : rect.y + rect.h / 2 - 7;
    draw::text(r, f->body, title, rect.x + 34, text_y, tc.text_primary);

    if (subtitle) {
        draw::text(r, f->small, subtitle, rect.x + 34, text_y + 16, tc.text_secondary);
    }

    // Bottom divider
    draw::line(r, rect.x + 34, rect.y + rect.h - 1, rect.x + rect.w - 8,
               rect.y + rect.h - 1, tc.divider);
}

// ── Avatar ──────────────────────────────────────────────────────

void avatar(SDL_Renderer* r, const ThemeColors& tc,
            int cx, int cy, int radius, Icon icon) {
    draw::filled_circle(r, cx, cy, radius, tc.accent_dim);
    draw::circle(r, cx, cy, radius, tc.border);
    draw::icon(r, icon, cx, cy, radius - 4, tc.accent);
}

// ── Tooltip ─────────────────────────────────────────────────────

void tooltip(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
             int x, int y, const char* text) {
    SDL_Point sz = draw::text_size(f->small, text);
    int pw = sz.x + 16;
    int ph = sz.y + 8;
    draw::filled_rounded_rect(r, {x, y, pw, ph}, 4, tc.bg_primary);
    draw::rounded_rect(r, {x, y, pw, ph}, 4, tc.border);
    draw::text(r, f->small, text, x + 8, y + 4, tc.text_primary);
}

// ── Status Dot ──────────────────────────────────────────────────

void status_dot(SDL_Renderer* r, const ThemeColors& tc,
                int cx, int cy, StatusType status) {
    SDL_Color col;
    switch (status) {
        case StatusType::Online:  col = tc.success; break;
        case StatusType::Away:    col = tc.warning; break;
        case StatusType::Busy:    col = tc.error;   break;
        case StatusType::Offline: col = tc.text_disabled; break;
    }
    draw::filled_circle(r, cx, cy, 4, col);
}

} // namespace widget
