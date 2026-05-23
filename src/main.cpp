#include "ui.h"
#include "app_registry.h"
#include "process.h"
#include "vfs.h"
#include "event_bus.h"
#include "shortcuts.h"
#include "context_menu.h"
#include "theme.h"
#include "audio.h"
#include "lockscreen.h"
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

// ── Launch autostart apps via ProcessManager ────────────────────

static void launch_autostart_apps(ProcessManager& pm, AppRegistry& registry,
                                   WindowManager& wm,
                                   int screen_w, int screen_h) {
    for (auto* m : registry.list_apps()) {
        if (m->autostart) {
            pm.spawn(m->app_id, registry, wm, screen_w, screen_h);
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

// ── Handle sidebar click ────────────────────────────────────────

static bool handle_sidebar_click(int mx, int my, int screen_w, int screen_h,
                                  AppRegistry& registry, WindowManager& wm) {
    std::string app_id = sidebar_app_at(mx, my, screen_h);
    if (app_id.empty()) return false;

    if (!registry.has_app(app_id)) return false;

    // Same logic as dock: toggle or launch
    if (registry.is_running(app_id)) {
        int wid = registry.find_window_for_app(app_id);
        auto* win = wm.find_window(wid);
        if (win) {
            if (win->minimized) {
                wm.restore_from_dock(wid, screen_w, screen_h);
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

// (sync is now handled by ProcessManager::sync)

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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

    // Virtual filesystem + settings
    FileSystem vfs;
    SystemSettings sys_settings(vfs);

    // Event bus + clipboard + notifications
    EventBus bus;
    Clipboard clipboard;
    clipboard.set_event_bus(&bus);
    NotificationManager notifications;
    notifications.set_event_bus(&bus);

    // Audio manager
    AudioManager audio;
    audio.init();
    audio.play(SystemSound::Startup);

    // Lock screen / login
    LockScreen lockscreen;
    {
        const char* home = getenv("HOME");
        std::string cred_path = std::string(home ? home : "/tmp") + "/.heros/credentials.conf";
        lockscreen.init(cred_path);
        lockscreen.set_audio(&audio);
    }

    // App registry — single source of truth for installed apps
    AppRegistry registry;
    registry.load_dynamic_apps();      // load .so plugins from ~/.heros/apps/
    register_builtin_apps(registry);   // fallback for any missing apps

    // Window manager + process manager
    WindowManager wm;
    ProcessManager pm;

    // Wire system services into registry so apps get them via context
    registry.set_system(&pm, &vfs, &sys_settings, &bus, &clipboard, &notifications, &audio);

    // Theme manager
    ThemeManager theme_mgr;

    // Keyboard shortcuts
    ShortcutManager shortcuts;
    ContextMenu ctx_menu;

    // Launch autostart apps through process manager
    int sw, sh;
    SDL_GetWindowSize(window, &sw, &sh);
    launch_autostart_apps(pm, registry, wm, sw, sh);

    // Register global shortcuts
    shortcuts.bind("app.close", SDLK_w, KMOD_CTRL, [&]() {
        auto* fw = wm.focused_window();
        if (fw) wm.close_window(fw->id);
    });
    shortcuts.bind("app.minimize", SDLK_m, KMOD_CTRL, [&]() {
        auto* fw = wm.focused_window();
        if (fw) wm.minimize(fw->id);
    });
    shortcuts.bind("wm.task_manager", SDLK_DELETE, KMOD_CTRL | KMOD_ALT, [&]() {
        int sw2, sh2;
        SDL_GetWindowSize(window, &sw2, &sh2);
        registry.launch("com.heros.taskmanager", wm, sw2, sh2);
    });
    shortcuts.bind("wm.settings", SDLK_COMMA, KMOD_CTRL, [&]() {
        int sw2, sh2;
        SDL_GetWindowSize(window, &sw2, &sh2);
        registry.launch("com.heros.settings", wm, sw2, sh2);
    });
    shortcuts.bind("wm.files", SDLK_e, KMOD_CTRL, [&]() {
        int sw2, sh2;
        SDL_GetWindowSize(window, &sw2, &sh2);
        registry.launch("com.heros.files", wm, sw2, sh2);
    });
    shortcuts.bind("wm.terminal", SDLK_t, KMOD_CTRL | KMOD_ALT, [&]() {
        int sw2, sh2;
        SDL_GetWindowSize(window, &sw2, &sh2);
        registry.launch("com.heros.terminal", wm, sw2, sh2);
    });
    shortcuts.bind("session.lock", SDLK_l, KMOD_CTRL | KMOD_GUI, [&]() {
        lockscreen.lock();
    });

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE
                && (event.key.keysym.mod & KMOD_CTRL))
                running = false;

            // Lock screen consumes all events when active
            if (lockscreen.is_locked()) {
                lockscreen.handle_event(event);
                continue;
            }

            // Global keyboard shortcuts
            if (event.type == SDL_KEYDOWN) {
                if (shortcuts.handle_key(event.key.keysym.sym, event.key.keysym.mod))
                    continue;
            }

            // Context menu clicks take priority
            if (event.type == SDL_MOUSEBUTTONDOWN && ctx_menu.is_open()) {
                if (ctx_menu.handle_click(event.button.x, event.button.y))
                    continue;
            }

            // Right-click opens context menu on desktop
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                int mx = event.button.x, my = event.button.y;
                int w2, h2;
                SDL_GetWindowSize(window, &w2, &h2);

                // Only show desktop context menu if not clicking on a window
                std::vector<MenuItem> items = {
                    {"Terminal", "Ctrl+Alt+T", Icon::Grid, true, false, [&]() {
                        registry.launch("com.heros.terminal", wm, w2, h2);
                    }},
                    {"Settings", "Ctrl+,", Icon::Gear, true, false, [&]() {
                        registry.launch("com.heros.settings", wm, w2, h2);
                    }},
                    {"Files", "Ctrl+E", Icon::Book, true, false, [&]() {
                        registry.launch("com.heros.files", wm, w2, h2);
                    }},
                    {"Task Manager", "Ctrl+Alt+Del", Icon::Grid, true, true, [&]() {
                        registry.launch("com.heros.taskmanager", wm, w2, h2);
                    }},
                    {"About HerOS", "", Icon::Ring, true, false, [&]() {
                        registry.launch("com.heros.settings", wm, w2, h2);
                    }},
                };
                ctx_menu.open(mx, my, std::move(items));
                continue;
            }

            // Context menu hover tracking
            if (event.type == SDL_MOUSEMOTION && ctx_menu.is_open()) {
                ctx_menu.on_mouse_move(event.motion.x, event.motion.y);
            }

            // Dock + sidebar clicks — check before WM so they take priority
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (handle_dock_click(event.button.x, event.button.y, w, h,
                                      registry, wm)) {
                    continue;
                }
                if (handle_sidebar_click(event.button.x, event.button.y, w, h,
                                          registry, wm)) {
                    continue;
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

        // Process manager: tick services, sync with WM
        pm.tick(wm);
        pm.sync(wm, registry);

        // Notification manager: expire old toasts
        notifications.tick(SDL_GetTicks());

        // 1. Render scene to frost target
        SDL_SetRenderTarget(renderer, frost.scene_target());
        render_background(renderer, wallpaper, w, h);
        render_geometric_overlay(renderer, w, h);

        // 2. Process blur
        frost.process_blur(renderer);

        // 3. Render scene to screen
        frost.render_scene(renderer);

        // 4. Render UI components
        RenderCtx ctx = {renderer, &frost, &fonts, w, h, &theme_mgr};
        render_topbar(ctx);
        render_left_sidebar(ctx, registry);
        render_right_sidebar(ctx);
        wm.render(ctx);
        render_dock(ctx, wm, registry);

        // Render context menu
        ctx_menu.render(renderer, &fonts);

        // Render toast notifications on top of everything
        notifications.render(renderer, &fonts, w);

        // Lock screen renders on top of ALL UI
        lockscreen.render(renderer, &frost, &fonts, w, h, wallpaper);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Unload dynamic app plugins before tearing down SDL
    registry.unload_all_dynamic();

    audio.cleanup();
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
