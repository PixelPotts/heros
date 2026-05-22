#include "draw.h"
#include <cmath>
#include <cstring>

// ── Fonts ───────────────────────────────────────────────────────

bool Fonts::load(const char* path) {
    brand  = TTF_OpenFont(path, 14);
    title  = TTF_OpenFont(path, 15);
    body   = TTF_OpenFont(path, 13);
    small  = TTF_OpenFont(path, 11);
    widget = TTF_OpenFont(path, 22);
    large  = TTF_OpenFont(path, 36);
    return brand && title && body && small && widget && large;
}

void Fonts::cleanup() {
    TTF_Font** all[] = {&brand,&title,&body,&small,&widget,&large};
    for (auto* fp : all) { if (*fp) TTF_CloseFont(*fp); *fp = nullptr; }
}

// ── Text ────────────────────────────────────────────────────────

static void render_surf(SDL_Renderer* r, SDL_Surface* s, int x, int y) {
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(r, t, nullptr, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

void draw::text(SDL_Renderer* r, TTF_Font* f, const char* s,
                int x, int y, SDL_Color c) {
    if (!f || !s || !*s) return;
    render_surf(r, TTF_RenderUTF8_Blended(f, s, c), x, y);
}

void draw::text_centered(SDL_Renderer* r, TTF_Font* f, const char* s,
                          int cx, int y, SDL_Color c) {
    if (!f || !s || !*s) return;
    int tw, th;
    TTF_SizeUTF8(f, s, &tw, &th);
    text(r, f, s, cx - tw / 2, y, c);
}

void draw::text_right(SDL_Renderer* r, TTF_Font* f, const char* s,
                       int rx, int y, SDL_Color c) {
    if (!f || !s || !*s) return;
    int tw, th;
    TTF_SizeUTF8(f, s, &tw, &th);
    text(r, f, s, rx - tw, y, c);
}

void draw::text_spaced(SDL_Renderer* r, TTF_Font* f, const char* s,
                        int x, int y, SDL_Color c, int extra) {
    if (!f || !s) return;
    char buf[2] = {0, 0};
    int cx = x;
    for (const char* p = s; *p; p++) {
        buf[0] = *p;
        render_surf(r, TTF_RenderUTF8_Blended(f, buf, c), cx, y);
        int cw;
        TTF_SizeUTF8(f, buf, &cw, nullptr);
        cx += cw + extra;
    }
}

SDL_Point draw::text_size(TTF_Font* f, const char* s) {
    SDL_Point p = {0, 0};
    if (f && s) TTF_SizeUTF8(f, s, &p.x, &p.y);
    return p;
}

// ── Shapes ──────────────────────────────────────────────────────

void draw::filled_circle(SDL_Renderer* r, int cx, int cy, int rad, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrt((double)(rad * rad - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void draw::circle(SDL_Renderer* r, int cx, int cy, int rad, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    int x = 0, y = rad, d = 1 - rad;
    while (x <= y) {
        SDL_RenderDrawPoint(r, cx + x, cy + y);
        SDL_RenderDrawPoint(r, cx - x, cy + y);
        SDL_RenderDrawPoint(r, cx + x, cy - y);
        SDL_RenderDrawPoint(r, cx - x, cy - y);
        SDL_RenderDrawPoint(r, cx + y, cy + x);
        SDL_RenderDrawPoint(r, cx - y, cy + x);
        SDL_RenderDrawPoint(r, cx + y, cy - x);
        SDL_RenderDrawPoint(r, cx - y, cy - x);
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

void draw::dotted_circle(SDL_Renderer* r, int cx, int cy, int rad,
                          int dots, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < dots; i++) {
        float a = 2.0f * (float)M_PI * i / dots;
        int px = cx + (int)(rad * cosf(a));
        int py = cy + (int)(rad * sinf(a));
        SDL_RenderDrawPoint(r, px, py);
        SDL_RenderDrawPoint(r, px + 1, py);
        SDL_RenderDrawPoint(r, px, py + 1);
        SDL_RenderDrawPoint(r, px + 1, py + 1);
    }
}

void draw::rounded_rect(SDL_Renderer* r, SDL_Rect rect, int rad, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

    int x1 = rect.x, y1 = rect.y;
    int x2 = rect.x + rect.w - 1, y2 = rect.y + rect.h - 1;

    SDL_RenderDrawLine(r, x1 + rad, y1, x2 - rad, y1);
    SDL_RenderDrawLine(r, x1 + rad, y2, x2 - rad, y2);
    SDL_RenderDrawLine(r, x1, y1 + rad, x1, y2 - rad);
    SDL_RenderDrawLine(r, x2, y1 + rad, x2, y2 - rad);

    auto corner = [&](int cx, int cy, int sx, int sy) {
        int px = 0, py = rad, d = 1 - rad;
        while (px <= py) {
            SDL_RenderDrawPoint(r, cx + sx * px, cy + sy * py);
            SDL_RenderDrawPoint(r, cx + sx * py, cy + sy * px);
            if (d < 0) d += 2 * px + 3;
            else { d += 2 * (px - py) + 5; py--; }
            px++;
        }
    };
    corner(x1 + rad, y1 + rad, -1, -1);
    corner(x2 - rad, y1 + rad,  1, -1);
    corner(x1 + rad, y2 - rad, -1,  1);
    corner(x2 - rad, y2 - rad,  1,  1);
}

void draw::filled_rounded_rect(SDL_Renderer* r, SDL_Rect rect, int rad, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);

    // Three non-overlapping strips
    SDL_Rect rects[3] = {
        {rect.x + rad, rect.y, rect.w - 2 * rad, rect.h},
        {rect.x, rect.y + rad, rad, rect.h - 2 * rad},
        {rect.x + rect.w - rad, rect.y + rad, rad, rect.h - 2 * rad}
    };
    SDL_RenderFillRects(r, rects, 3);

    // Corner filled quarter circles
    auto fill_qc = [&](int cx, int cy, int sx, int sy) {
        for (int dy = 0; dy <= rad; dy++) {
            int dx = (int)sqrt((double)(rad * rad - dy * dy));
            SDL_RenderDrawLine(r, cx, cy + sy * dy, cx + sx * dx, cy + sy * dy);
        }
    };
    fill_qc(rect.x + rad,              rect.y + rad,              -1, -1);
    fill_qc(rect.x + rect.w - rad - 1, rect.y + rad,               1, -1);
    fill_qc(rect.x + rad,              rect.y + rect.h - rad - 1, -1,  1);
    fill_qc(rect.x + rect.w - rad - 1, rect.y + rect.h - rad - 1,  1,  1);
}

void draw::glow(SDL_Renderer* r, int cx, int cy, int max_rad, SDL_Color c) {
    for (int rad = max_rad; rad > 0; rad -= 2) {
        Uint8 a = (Uint8)(c.a * (1.0f - (float)rad / max_rad));
        filled_circle(r, cx, cy, rad, {c.r, c.g, c.b, a});
    }
}

void draw::line(SDL_Renderer* r, int x1, int y1, int x2, int y2, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

// ── Icons ───────────────────────────────────────────────────────
// All icons drawn centered at (x,y) within a sz×sz bounding box.

static void set_col(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void icon_bell(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int h = s * 4 / 10, w2 = s * 3 / 10;
    // Bell body
    SDL_RenderDrawLine(r, x, y - h, x - w2, y + h / 2);
    SDL_RenderDrawLine(r, x, y - h, x + w2, y + h / 2);
    SDL_RenderDrawLine(r, x - w2, y + h / 2, x + w2, y + h / 2);
    // Clapper
    SDL_RenderDrawPoint(r, x, y + h / 2 + 2);
    SDL_RenderDrawPoint(r, x - 1, y + h / 2 + 3);
    SDL_RenderDrawPoint(r, x + 1, y + h / 2 + 3);
}

static void icon_waveform(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int heights[] = {3, 6, 9, 6, 3};
    for (int i = 0; i < 5; i++) {
        int bx = x - 6 + i * 3;
        int bh = heights[i] * s / 20;
        SDL_RenderDrawLine(r, bx, y - bh, bx, y + bh);
    }
}

static void icon_grid(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int g = s / 4;
    for (int row = -1; row <= 1; row++)
        for (int col = -1; col <= 1; col++) {
            SDL_Rect rc = {x + col * g - 1, y + row * g - 1, 3, 3};
            SDL_RenderFillRect(r, &rc);
        }
}

static void icon_volume(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int h = s / 3;
    SDL_Rect sp = {x - s / 4, y - h / 2, s / 6, h};
    SDL_RenderFillRect(r, &sp);
    SDL_RenderDrawLine(r, x - s / 4 + s / 6, y - h / 2, x + s / 8, y - h);
    SDL_RenderDrawLine(r, x - s / 4 + s / 6, y + h / 2, x + s / 8, y + h);
    // Sound wave arcs
    for (int i = 1; i <= 2; i++) {
        int ar = s / 5 * i;
        int ax = x + s / 8 + 2;
        for (float a = -0.6f; a < 0.6f; a += 0.1f) {
            SDL_RenderDrawPoint(r, ax + (int)(ar * cosf(a)), y + (int)(ar * sinf(a)));
        }
    }
}

static void icon_power(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::circle(r, x, y + 1, s / 3, c);
    set_col(r, c);
    SDL_RenderDrawLine(r, x, y - s / 3 - 1, x, y + 1);
}

static void icon_flower(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    int pr = s / 5;
    int dist = s / 3;
    for (int i = 0; i < 6; i++) {
        float a = (float)M_PI * 2 * i / 6;
        draw::circle(r, x + (int)(dist * cosf(a)), y + (int)(dist * sinf(a)), pr, c);
    }
    draw::filled_circle(r, x, y, pr / 2 + 1, c);
}

static void icon_book(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s * 3 / 8, hh = s * 4 / 10;
    SDL_Rect rc = {x - hw, y - hh, 2 * hw, 2 * hh};
    SDL_RenderDrawRect(r, &rc);
    SDL_RenderDrawLine(r, x, y - hh, x, y + hh);
}

static void icon_journal(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s * 3 / 8, hh = s * 4 / 10;
    SDL_Rect rc = {x - hw, y - hh, 2 * hw, 2 * hh};
    SDL_RenderDrawRect(r, &rc);
    for (int i = -1; i <= 1; i++)
        SDL_RenderDrawLine(r, x - hw + 3, y + i * (hh / 2), x + hw - 3, y + i * (hh / 2));
}

static void icon_briefcase(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s * 4 / 10, hh = s * 3 / 10;
    SDL_Rect rc = {x - hw, y - hh + 2, 2 * hw, 2 * hh};
    SDL_RenderDrawRect(r, &rc);
    SDL_RenderDrawLine(r, x - s / 6, y - hh + 2, x - s / 6, y - hh - 2);
    SDL_RenderDrawLine(r, x + s / 6, y - hh + 2, x + s / 6, y - hh - 2);
    SDL_RenderDrawLine(r, x - s / 6, y - hh - 2, x + s / 6, y - hh - 2);
}

static void icon_sliders(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s * 3 / 8;
    for (int i = -1; i <= 1; i++) {
        int ly = y + i * (s / 4);
        SDL_RenderDrawLine(r, x - hw, ly, x + hw, ly);
        int dx = (i + 1) * s / 5 - s / 5;
        draw::filled_circle(r, x + dx, ly, 2, c);
    }
}

static void icon_compass(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::circle(r, x, y, s * 3 / 8, c);
    set_col(r, c);
    int d = s / 5;
    SDL_RenderDrawLine(r, x, y - d, x + d / 2, y);
    SDL_RenderDrawLine(r, x + d / 2, y, x, y + d);
    SDL_RenderDrawLine(r, x, y + d, x - d / 2, y);
    SDL_RenderDrawLine(r, x - d / 2, y, x, y - d);
}

static void icon_people(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    int r1 = s / 6;
    draw::circle(r, x - s / 6, y - s / 5, r1, c);
    draw::circle(r, x + s / 6, y - s / 5, r1, c);
    set_col(r, c);
    // Bodies
    SDL_RenderDrawLine(r, x - s / 3, y + s / 4, x - s / 6, y + 1);
    SDL_RenderDrawLine(r, x - s / 6, y + 1, x, y + s / 4);
    SDL_RenderDrawLine(r, x, y + s / 4, x + s / 6, y + 1);
    SDL_RenderDrawLine(r, x + s / 6, y + 1, x + s / 3, y + s / 4);
}

static void icon_gear(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::circle(r, x, y, s / 4, c);
    set_col(r, c);
    for (int i = 0; i < 8; i++) {
        float a = (float)M_PI * 2 * i / 8;
        int ix = x + (int)(s * 3 / 8 * cosf(a));
        int iy = y + (int)(s * 3 / 8 * sinf(a));
        SDL_RenderDrawLine(r, x + (int)(s / 4 * cosf(a)),
                              y + (int)(s / 4 * sinf(a)), ix, iy);
    }
}

static void icon_star(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int outer = s * 4 / 10, inner = s / 5;
    float start = -(float)M_PI / 2;
    int pts[10][2];
    for (int i = 0; i < 10; i++) {
        float a = start + (float)M_PI * 2 * i / 10;
        int rad = (i % 2 == 0) ? outer : inner;
        pts[i][0] = x + (int)(rad * cosf(a));
        pts[i][1] = y + (int)(rad * sinf(a));
    }
    for (int i = 0; i < 10; i++)
        SDL_RenderDrawLine(r, pts[i][0], pts[i][1],
                              pts[(i + 1) % 10][0], pts[(i + 1) % 10][1]);
}

static void icon_sparkle(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s * 3 / 8;
    SDL_RenderDrawLine(r, x, y - d, x, y + d);
    SDL_RenderDrawLine(r, x - d, y, x + d, y);
    int d2 = d * 2 / 3;
    SDL_RenderDrawLine(r, x - d2, y - d2, x + d2, y + d2);
    SDL_RenderDrawLine(r, x + d2, y - d2, x - d2, y + d2);
}

static void icon_moon(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int rad = s * 3 / 8;
    int inner = rad * 3 / 4;
    int ox = rad / 3;
    // Draw crescent by plotting points on outer circle that are outside inner circle
    for (float a = 0; a < 2 * (float)M_PI; a += 0.04f) {
        int px = (int)(rad * cosf(a));
        int py = (int)(rad * sinf(a));
        int dx = px - ox, dy = py + rad / 4;
        if (dx * dx + dy * dy > inner * inner)
            SDL_RenderDrawPoint(r, x + px, y + py);
    }
}

static void icon_target(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::circle(r, x, y, s * 4 / 10, c);
    draw::circle(r, x, y, s * 2 / 10, c);
    draw::filled_circle(r, x, y, 2, c);
}

static void icon_lotus(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int h = s * 3 / 8;
    // Center petal
    draw::circle(r, x, y - h / 3, h / 2, c);
    // Side petals
    draw::circle(r, x - h * 2 / 3, y, h / 2, c);
    draw::circle(r, x + h * 2 / 3, y, h / 2, c);
}

static void icon_mountain(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int b = s * 3 / 8;
    SDL_RenderDrawLine(r, x, y - b, x - b, y + b);
    SDL_RenderDrawLine(r, x, y - b, x + b, y + b);
    SDL_RenderDrawLine(r, x - b, y + b, x + b, y + b);
    // Small peak
    int s2 = b / 2;
    SDL_RenderDrawLine(r, x + b / 2, y, x + b / 2 - s2, y + b);
    SDL_RenderDrawLine(r, x + b / 2, y, x + b / 2 + s2, y + b);
}

static void icon_trash(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s / 4, hh = s * 3 / 8;
    SDL_Rect rc = {x - hw, y - hh + 3, 2 * hw, 2 * hh - 3};
    SDL_RenderDrawRect(r, &rc);
    SDL_RenderDrawLine(r, x - hw - 2, y - hh + 2, x + hw + 2, y - hh + 2);
    SDL_RenderDrawLine(r, x - 2, y - hh + 2, x - 2, y - hh);
    SDL_RenderDrawLine(r, x + 2, y - hh + 2, x + 2, y - hh);
    SDL_RenderDrawLine(r, x - 2, y - hh, x + 2, y - hh);
}

static void icon_pen(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s * 3 / 8;
    SDL_RenderDrawLine(r, x - d, y + d, x + d, y - d);
    SDL_RenderDrawLine(r, x + d - 2, y - d, x + d, y - d + 2);
    SDL_RenderDrawPoint(r, x - d, y + d + 1);
}

static void icon_image(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s * 3 / 8, hh = s * 3 / 10;
    SDL_Rect rc = {x - hw, y - hh, 2 * hw, 2 * hh};
    SDL_RenderDrawRect(r, &rc);
    // Small mountain inside
    SDL_RenderDrawLine(r, x - hw / 2, y + hh / 2, x, y - hh / 3);
    SDL_RenderDrawLine(r, x, y - hh / 3, x + hw / 2, y + hh / 2);
}

static void icon_pin(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::circle(r, x, y - s / 6, s / 4, c);
    set_col(r, c);
    SDL_RenderDrawLine(r, x, y + s / 6, x, y + s * 3 / 8);
}

static void icon_check(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s / 4;
    SDL_RenderDrawLine(r, x - d, y, x - d / 3, y + d);
    SDL_RenderDrawLine(r, x - d / 3, y + d, x + d, y - d);
}

static void icon_lock(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int hw = s / 4, bh = s / 4, top = s / 4;
    SDL_Rect rc = {x - hw, y, 2 * hw, bh};
    SDL_RenderDrawRect(r, &rc);
    // Shackle
    for (float a = 0; a < (float)M_PI; a += 0.15f) {
        SDL_RenderDrawPoint(r, x + (int)(hw * 2 / 3 * cosf(a)),
                               y - (int)(top * sinf(a)));
    }
}

static void icon_dots(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::filled_circle(r, x - s / 4, y, 2, c);
    draw::filled_circle(r, x, y, 2, c);
    draw::filled_circle(r, x + s / 4, y, 2, c);
}

static void icon_close(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s / 4;
    SDL_RenderDrawLine(r, x - d, y - d, x + d, y + d);
    SDL_RenderDrawLine(r, x + d, y - d, x - d, y + d);
}

static void icon_minimize(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    SDL_RenderDrawLine(r, x - s / 4, y, x + s / 4, y);
}

static void icon_maximize(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s / 4;
    SDL_Rect rc = {x - d, y - d, 2 * d, 2 * d};
    SDL_RenderDrawRect(r, &rc);
}

static void icon_chevron_up(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s / 4;
    SDL_RenderDrawLine(r, x - d, y + d / 2, x, y - d / 2);
    SDL_RenderDrawLine(r, x, y - d / 2, x + d, y + d / 2);
}

static void icon_box(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    set_col(r, c);
    int d = s * 3 / 8;
    SDL_Rect rc = {x - d, y - d, 2 * d, 2 * d};
    SDL_RenderDrawRect(r, &rc);
    SDL_RenderDrawLine(r, x - d, y - d / 2, x + d, y - d / 2);
}

static void icon_ring(SDL_Renderer* r, int x, int y, int s, SDL_Color c) {
    draw::circle(r, x, y, s * 3 / 8, c);
}

void draw::icon(SDL_Renderer* r, Icon type, int x, int y, int sz, SDL_Color c) {
    switch (type) {
        case Icon::Bell:      icon_bell(r, x, y, sz, c);      break;
        case Icon::Waveform:  icon_waveform(r, x, y, sz, c);  break;
        case Icon::Grid:      icon_grid(r, x, y, sz, c);      break;
        case Icon::Volume:    icon_volume(r, x, y, sz, c);     break;
        case Icon::Power:     icon_power(r, x, y, sz, c);     break;
        case Icon::Flower:    icon_flower(r, x, y, sz, c);    break;
        case Icon::Book:      icon_book(r, x, y, sz, c);      break;
        case Icon::Journal:   icon_journal(r, x, y, sz, c);   break;
        case Icon::Briefcase: icon_briefcase(r, x, y, sz, c); break;
        case Icon::Sliders:   icon_sliders(r, x, y, sz, c);   break;
        case Icon::Compass:   icon_compass(r, x, y, sz, c);   break;
        case Icon::People:    icon_people(r, x, y, sz, c);    break;
        case Icon::Gear:      icon_gear(r, x, y, sz, c);      break;
        case Icon::Star:      icon_star(r, x, y, sz, c);      break;
        case Icon::Sparkle:   icon_sparkle(r, x, y, sz, c);   break;
        case Icon::Moon:      icon_moon(r, x, y, sz, c);      break;
        case Icon::Target:    icon_target(r, x, y, sz, c);    break;
        case Icon::Lotus:     icon_lotus(r, x, y, sz, c);     break;
        case Icon::Mountain:  icon_mountain(r, x, y, sz, c);  break;
        case Icon::Trash:     icon_trash(r, x, y, sz, c);     break;
        case Icon::Pen:       icon_pen(r, x, y, sz, c);       break;
        case Icon::Image:     icon_image(r, x, y, sz, c);     break;
        case Icon::Pin:       icon_pin(r, x, y, sz, c);       break;
        case Icon::Check:     icon_check(r, x, y, sz, c);     break;
        case Icon::Lock:      icon_lock(r, x, y, sz, c);      break;
        case Icon::Dots:      icon_dots(r, x, y, sz, c);      break;
        case Icon::Close:     icon_close(r, x, y, sz, c);     break;
        case Icon::Minimize:  icon_minimize(r, x, y, sz, c);  break;
        case Icon::Maximize:  icon_maximize(r, x, y, sz, c);  break;
        case Icon::ChevronUp: icon_chevron_up(r, x, y, sz, c);break;
        case Icon::Box:       icon_box(r, x, y, sz, c);       break;
        case Icon::Ring:      icon_ring(r, x, y, sz, c);      break;
    }
}
