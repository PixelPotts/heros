#include "ui.h"
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

// ── Dock click detection ────────────────────────────────────────

static int dock_icon_at(int mx, int my, int screen_w, int screen_h) {
    int dw = 380, dh = 48;
    int dx = (screen_w - dw) / 2;
    int dy = screen_h - 58;

    // Check if click is within dock bounds
    if (mx < dx || mx >= dx + dw || my < dy || my >= dy + dh)
        return -1;

    int num = 7;
    int spacing = (dw - 24) / num;
    int ix = dx + 12 + spacing / 2;

    for (int i = 0; i < num; i++) {
        if (mx >= ix - spacing / 2 && mx < ix + spacing / 2)
            return i;
        ix += spacing;
    }
    return -1;
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
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
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

    // Window manager
    WindowManager wm;
    setup_default_windows(wm, INIT_W, INIT_H);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;

            // Dock click detection — check before WM so dock takes priority
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                int dock_idx = dock_icon_at(event.button.x, event.button.y, w, h);
                if (dock_idx == 3) { // Journal icon
                    // Find journal window and restore if minimized, or minimize if visible
                    for (auto& win : wm.windows()) {
                        if (win.icon == Icon::Journal) {
                            if (win.minimized) {
                                wm.restore_from_dock(win.id, w, h);
                            } else {
                                wm.minimize(win.id);
                            }
                            break;
                        }
                    }
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
        render_dock(ctx, wm);

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
