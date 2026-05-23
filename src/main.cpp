#include "ui.h"
#include "app_registry.h"
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstdlib>

static const int INIT_W = 1280;
static const int INIT_H = 720;
static const char* WALLPAPER_URL =
    "https://images.unsplash.com/photo-1494500764479-0c8f2919a3d8?w=1920&q=80";
static const char* WALLPAPER_PATH = "assets/wallpaper.jpg";
static const char* FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

static SDL_Texture* load_wallpaper(SDL_Renderer* r) {
    FILE* f = fopen(WALLPAPER_PATH, "r");
    if (!f) {
        fprintf(stderr, "Downloading wallpaper...\n");
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "mkdir -p assets && curl -sL -o '%s' '%s'",
                 WALLPAPER_PATH, WALLPAPER_URL);
        system(cmd);
    } else {
        fclose(f);
    }

    SDL_Surface* surf = IMG_Load(WALLPAPER_PATH);
    if (!surf) {
        fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

// ── Launch autostart apps ───────────────────────────────────────

static void launch_autostart_apps(AppRegistry& registry, WindowManager& wm,
                                   int screen_w, int screen_h) {
    for (auto* m : registry.list_apps()) {
        if (m->autostart) {
            registry.launch(m->app_id, wm, screen_w, screen_h);
        }
    }
}

// ── Handle dock click — works for any registered app ────────────

static bool handle_dock_click(int mx, int my, int screen_w, int screen_h,
                               AppRegistry& registry, WindowManager& wm) {
    std::string app_id = dock_app_at(mx, my, screen_w, screen_h, wm, registry);
    if (app_id.empty()) return false;

    // If running: toggle minimize/focus. If not running: launch.
    if (registry.is_running(app_id)) {
        int wid = registry.find_window_for_app(app_id);
        auto* win = wm.find_window(wid);
        if (win) {
            if (win->minimized) {
                wm.restore_from_dock(wid, screen_w, screen_h);
            } else if (win->active) {
                wm.minimize(wid);
            } else {
                wm.bring_to_front(wid);
                wm.set_focus(wid);
            }
        }
    } else {
        registry.launch(app_id, wm, screen_w, screen_h);
    }
    return true;
}

// ── Track window closures ───────────────────────────────────────

static void sync_running_state(AppRegistry& registry, const WindowManager& wm) {
    // Simple approach: rebuild from current WM state each frame would be expensive.
    // Instead, detect closed windows by checking if tracked windows still exist.
    // This is called once per frame — cheap since running_ is small.
    std::vector<int> to_remove;
    // We need access to running instances — use the public API
    // Check each running app's window still exists in WM
    for (auto* m : registry.list_apps()) {
        int wid = registry.find_window_for_app(m->app_id);
        if (wid >= 0 && !wm.find_window(wid)) {
            to_remove.push_back(wid);
        }
    }
    for (int wid : to_remove) {
        registry.on_window_closed(wid);
    }
}

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "HerOS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        INIT_W, INIT_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return 1;
    }

    SDL_StartTextInput();

    // Load resources
    Fonts fonts;
    if (!fonts.load(FONT_PATH)) {
        fprintf(stderr, "Failed to load fonts\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return 1;
    }

    SDL_Texture* wallpaper = load_wallpaper(renderer);

    FrostRenderer frost;
    int prev_w = INIT_W, prev_h = INIT_H;
    frost.init(renderer, prev_w, prev_h);

    // App registry — single source of truth for installed apps
    AppRegistry registry;
    register_builtin_apps(registry);

    // Window manager
    WindowManager wm;

    // Launch autostart apps (replaces hardcoded setup_default_windows)
    int sw, sh;
    SDL_GetWindowSize(window, &sw, &sh);
    launch_autostart_apps(registry, wm, sw, sh);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE
                && (event.key.keysym.mod & KMOD_CTRL))
                running = false;

            // Dock click — check before WM so dock takes priority
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (handle_dock_click(event.button.x, event.button.y, w, h,
                                      registry, wm)) {
                    continue; // Don't pass to WM
                }
            }

            wm.handle_event(event);
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        if (w != prev_w || h != prev_h) {
            frost.resize(renderer, w, h);
            prev_w = w; prev_h = h;
        }

        // Sync registry with WM (detect closed windows)
        sync_running_state(registry, wm);

        // 1. Render scene to frost target
        SDL_SetRenderTarget(renderer, frost.scene_target());
        render_background(renderer, wallpaper, w, h);
        render_geometric_overlay(renderer, w, h);

        // 2. Process blur
        frost.process_blur(renderer);

        // 3. Render scene to screen
        frost.render_scene(renderer);

        // 4. Render UI components
        RenderCtx ctx = {renderer, &frost, &fonts, w, h};
        render_topbar(ctx);
        render_left_sidebar(ctx);
        render_right_sidebar(ctx);
        wm.render(ctx);
        render_dock(ctx, wm, registry);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (wallpaper) SDL_DestroyTexture(wallpaper);
    frost.cleanup();
    fonts.cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
