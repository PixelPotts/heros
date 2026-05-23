#pragma once
#include "draw.h"
#include "theme.h"
#include <string>
#include <vector>
#include <functional>

// ── Widget Helpers ──────────────────────────────────────────────
// Stateless drawing functions that use the theme for consistent visuals.

namespace widget {

// ── Card ────────────────────────────────────────────────────────

void card(SDL_Renderer* r, const ThemeColors& tc, SDL_Rect rect, int radius = 8);

// ── Button ──────────────────────────────────────────────────────

enum class ButtonStyle { Primary, Secondary, Ghost, Danger };

struct ButtonResult {
    bool hovered;
    bool clicked;
};

void button(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
            SDL_Rect rect, const char* label, ButtonStyle style = ButtonStyle::Primary);

// ── Toggle Switch ───────────────────────────────────────────────

void toggle(SDL_Renderer* r, const ThemeColors& tc, int x, int y, bool on);

// ── Progress Bar ────────────────────────────────────────────────

void progress_bar(SDL_Renderer* r, const ThemeColors& tc, SDL_Rect rect,
                  float pct, SDL_Color fill_color = {0, 0, 0, 0});

// ── Badge / Pill ────────────────────────────────────────────────

void badge(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
           int x, int y, const char* text, SDL_Color bg_color = {0, 0, 0, 0});

// ── Separator ───────────────────────────────────────────────────

void separator(SDL_Renderer* r, const ThemeColors& tc, int x, int y, int w);

// ── Section Header ──────────────────────────────────────────────

void section_header(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
                    int x, int y, const char* title);

// ── Icon Button ─────────────────────────────────────────────────

void icon_button(SDL_Renderer* r, const ThemeColors& tc,
                 int cx, int cy, int size, Icon icon, bool active = false);

// ── Tab Bar ─────────────────────────────────────────────────────

struct TabItem {
    const char* label;
    bool active;
};

void tab_bar(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
             int x, int y, int w, const TabItem* tabs, int count);

// ── List Item ───────────────────────────────────────────────────

void list_item(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
               SDL_Rect rect, Icon icon, const char* title,
               const char* subtitle = nullptr, bool selected = false);

// ── Avatar ──────────────────────────────────────────────────────

void avatar(SDL_Renderer* r, const ThemeColors& tc,
            int cx, int cy, int radius, Icon icon);

// ── Tooltip ─────────────────────────────────────────────────────

void tooltip(SDL_Renderer* r, const Fonts* f, const ThemeColors& tc,
             int x, int y, const char* text);

// ── Status Dot ──────────────────────────────────────────────────

enum class StatusType { Online, Away, Busy, Offline };

void status_dot(SDL_Renderer* r, const ThemeColors& tc,
                int cx, int cy, StatusType status);

} // namespace widget
