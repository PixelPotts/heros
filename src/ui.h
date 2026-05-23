#pragma once
#include "draw.h"
#include "frost.h"
#include "window.h"
#include "theme.h"

class AnimationManager;

struct RenderCtx {
    SDL_Renderer* r;
    FrostRenderer* frost;
    Fonts* fonts;
    int w, h;
    const ThemeManager* theme = nullptr;
    AnimationManager* animations = nullptr;
};

class AppRegistry;  // forward declare

void render_background(SDL_Renderer* r, SDL_Texture* bg, int w, int h);
void render_geometric_overlay(SDL_Renderer* r, int w, int h);
void render_topbar(const RenderCtx& ctx);
void render_left_sidebar(const RenderCtx& ctx, const AppRegistry& registry);

// Sidebar click detection — returns app_id or "" if none
std::string sidebar_app_at(int mx, int my, int screen_h);
void render_right_sidebar(const RenderCtx& ctx);
void render_dock(const RenderCtx& ctx, const WindowManager& wm,
                 const AppRegistry& registry);

// Dock hit detection — returns app_id of clicked icon, or "" if none
std::string dock_app_at(int mx, int my, int screen_w, int screen_h,
                        const WindowManager& wm, const AppRegistry& registry);
