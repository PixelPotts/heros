#pragma once
#include "draw.h"
#include "frost.h"
#include "window.h"

struct RenderCtx {
    SDL_Renderer* r;
    FrostRenderer* frost;
    Fonts* fonts;
    int w, h;
};

void render_background(SDL_Renderer* r, SDL_Texture* bg, int w, int h);
void render_geometric_overlay(SDL_Renderer* r, int w, int h);
void render_topbar(const RenderCtx& ctx);
void render_left_sidebar(const RenderCtx& ctx);
void render_right_sidebar(const RenderCtx& ctx);
void render_dock(const RenderCtx& ctx, const WindowManager& wm);
void setup_default_windows(WindowManager& wm, int screen_w, int screen_h);
