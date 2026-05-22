#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct Fonts {
    TTF_Font* brand   = nullptr;  // 14
    TTF_Font* title   = nullptr;  // 15
    TTF_Font* body    = nullptr;  // 13
    TTF_Font* small   = nullptr;  // 11
    TTF_Font* widget  = nullptr;  // 22
    TTF_Font* large   = nullptr;  // 36

    bool load(const char* path);
    void cleanup();
};

enum class Icon {
    Bell, Waveform, Grid, Volume, Power,
    Flower, Book, Journal, Briefcase, Sliders,
    Compass, People, Gear, Star, Sparkle,
    Moon, Target, Lotus, Mountain, Trash,
    Pen, Image, Pin, Check, Lock, Dots,
    Close, Minimize, Maximize, ChevronUp, Box, Ring
};

namespace draw {
    void text(SDL_Renderer* r, TTF_Font* f, const char* s, int x, int y, SDL_Color c);
    void text_centered(SDL_Renderer* r, TTF_Font* f, const char* s, int cx, int y, SDL_Color c);
    void text_right(SDL_Renderer* r, TTF_Font* f, const char* s, int rx, int y, SDL_Color c);
    void text_spaced(SDL_Renderer* r, TTF_Font* f, const char* s, int x, int y, SDL_Color c, int extra);
    SDL_Point text_size(TTF_Font* f, const char* s);

    void filled_circle(SDL_Renderer* r, int cx, int cy, int rad, SDL_Color c);
    void circle(SDL_Renderer* r, int cx, int cy, int rad, SDL_Color c);
    void dotted_circle(SDL_Renderer* r, int cx, int cy, int rad, int dots, SDL_Color c);
    void rounded_rect(SDL_Renderer* r, SDL_Rect rect, int rad, SDL_Color c);
    void filled_rounded_rect(SDL_Renderer* r, SDL_Rect rect, int rad, SDL_Color c);
    void glow(SDL_Renderer* r, int cx, int cy, int max_rad, SDL_Color c);
    void line(SDL_Renderer* r, int x1, int y1, int x2, int y2, SDL_Color c);

    void icon(SDL_Renderer* r, Icon type, int x, int y, int sz, SDL_Color c);
}
