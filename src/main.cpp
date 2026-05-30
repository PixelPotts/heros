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
#include "network.h"
#include "systray.h"
#include "launcher.h"
#include "workspaces.h"
#include "power.h"
#include "animation.h"
#include "file_assoc.h"
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstdlib>

static const int INIT_W = 1280;
static const int INIT_H = 720;
static const int TOPBAR_H = 36;
static const int DOCK_H   = 58;
static const char* FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

// wallpaper + autostart removed — terminal grid is the desktop

// ── Handle dock click — works for any registered app ────────────

// Forward declare for workspace tracking
static WorkspaceManager* g_workspaces = nullptr;

static bool handle_dock_click(int mx, int my, int screen_w, int screen_h,
                               AppRegistry& registry, ProcessManager& pm,
                               WindowManager& wm) {
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
        pm.spawn(app_id, registry, wm, screen_w, screen_h);
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

    // Grab keyboard so ALL keys are captured by HerOS (no Alt+Tab/Super leak)
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");

    SDL_Window* window = SDL_CreateWindow(
        "HerOS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        INIT_W, INIT_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP
        | SDL_WINDOW_INPUT_GRABBED
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

    // Grab all input — keyboard + mouse confined to this window
    SDL_SetWindowGrab(window, SDL_TRUE);

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

    SDL_Texture* wallpaper = nullptr; // no wallpaper — terminal grid is the background

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

    // Network manager
    NetworkManager network;
    network.init();

    // Theme manager
    ThemeManager theme_mgr;

    // File association manager
    FileAssocManager file_assoc;
    file_assoc.init();

    // Animation manager
    AnimationManager animations;
    wm.set_animations(&animations);

    // Power manager
    PowerManager power;
    power.init(&audio, &lockscreen);
    power.set_on_lock([&]() { lockscreen.lock(); });

    // Workspace manager
    WorkspaceManager workspaces;
    workspaces.init(4);
    g_workspaces = &workspaces;

    // App launcher (Spotlight-style)
    AppLauncher launcher;

    // System tray
    SystemTray systray;
    systray.init(&audio, &network, &notifications);

    // Keyboard shortcuts
    ShortcutManager shortcuts;
    ContextMenu ctx_menu;

    // Launch autostart apps through process manager
    int sw, sh;
    SDL_GetWindowSize(window, &sw, &sh);
    // launch_autostart_apps(pm, registry, wm, sw, sh); // disabled: terminal grid is the desktop

    // Spawn 15x10 terminal grid as desktop background
    {
        const int GRID_COLS = 15;
        const int GRID_ROWS = 10;
        int cell_w = sw / GRID_COLS;
        int cell_h = (sh - TOPBAR_H - DOCK_H) / GRID_ROWS;
        for (int row = 0; row < GRID_ROWS; row++) {
            for (int col = 0; col < GRID_COLS; col++) {
                SDL_Rect cell = {
                    col * cell_w,
                    TOPBAR_H + row * cell_h,
                    cell_w,
                    cell_h
                };
                registry.launch_at("com.heros.terminal", wm, cell,
                                   WF_NoTitleBar, sw, sh);
            }
        }
    }

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
    shortcuts.bind("wm.snap_left", SDLK_LEFT, KMOD_GUI, [&]() {
        auto* fw = wm.focused_window();
        if (fw) { int sw2, sh2; SDL_GetWindowSize(window, &sw2, &sh2); wm.snap_left(fw->id, sw2, sh2); }
    });
    shortcuts.bind("wm.snap_right", SDLK_RIGHT, KMOD_GUI, [&]() {
        auto* fw = wm.focused_window();
        if (fw) { int sw2, sh2; SDL_GetWindowSize(window, &sw2, &sh2); wm.snap_right(fw->id, sw2, sh2); }
    });
    shortcuts.bind("launcher.toggle", SDLK_SPACE, KMOD_CTRL, [&]() {
        launcher.toggle();
    });
    shortcuts.bind("wm.snap_maximize", SDLK_UP, KMOD_GUI, [&]() {
        auto* fw = wm.focused_window();
        if (fw) { int sw2, sh2; SDL_GetWindowSize(window, &sw2, &sh2); wm.toggle_maximize(fw->id, sw2, sh2); }
    });
    shortcuts.bind("ws.next", SDLK_RIGHT, KMOD_CTRL | KMOD_ALT, [&]() {
        workspaces.switch_next(wm);
    });
    shortcuts.bind("ws.prev", SDLK_LEFT, KMOD_CTRL | KMOD_ALT, [&]() {
        workspaces.switch_prev(wm);
    });
    shortcuts.bind("ws.switcher", SDLK_TAB, KMOD_GUI, [&]() {
        workspaces.toggle_switcher();
    });
    // Ctrl+Alt+1-4 for direct workspace switching
    for (int i = 0; i < 4; i++) {
        shortcuts.bind("ws.goto_" + std::to_string(i+1), SDLK_1 + i, KMOD_CTRL | KMOD_ALT, [&, i]() {
            workspaces.switch_to(i, wm);
        });
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE
                && (event.key.keysym.mod & KMOD_CTRL))
                running = false;

            // ESC alone: unmaximize focused window
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE
                && !(event.key.keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI))) {
                auto* fw = wm.focused_window();
                if (fw && fw->maximized) {
                    wm.restore(fw->id);
                }
            }

            // Re-grab keyboard when window gains focus (grab can fail if window wasn't focused)
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                SDL_SetWindowGrab(window, SDL_TRUE);
            }

            // Track activity for idle/dim
            if (event.type == SDL_MOUSEMOTION || event.type == SDL_KEYDOWN ||
                event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_TEXTINPUT) {
                power.on_activity();
            }

            // Lock screen consumes all events when active
            if (lockscreen.is_locked()) {
                lockscreen.handle_event(event);
                continue;
            }

            // Global keyboard shortcuts (before launcher so Ctrl+Space works)
            if (event.type == SDL_KEYDOWN) {
                if (shortcuts.handle_key(event.key.keysym.sym, event.key.keysym.mod))
                    continue;
            }

            // File association "Open With" dialog
            if (file_assoc.dialog_open()) {
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    int w2, h2;
                    SDL_GetWindowSize(window, &w2, &h2);
                    file_assoc.handle_click(event.button.x, event.button.y, registry, wm, w2, h2);
                    continue;
                }
                if (event.type == SDL_MOUSEMOTION) {
                    int w2, h2;
                    SDL_GetWindowSize(window, &w2, &h2);
                    file_assoc.on_mouse_move(event.motion.x, event.motion.y, w2, h2);
                }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    file_assoc.close_dialog();
                    continue;
                }
            }

            // Workspace switcher
            if (workspaces.switcher_open()) {
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    int w2, h2;
                    SDL_GetWindowSize(window, &w2, &h2);
                    workspaces.handle_switcher_click(event.button.x, event.button.y, wm, w2, h2);
                    continue;
                }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    workspaces.close_switcher();
                    continue;
                }
            }

            // App launcher consumes events when open
            if (launcher.is_open()) {
                int w2, h2;
                SDL_GetWindowSize(window, &w2, &h2);
                if (launcher.handle_event(event, registry, wm, w2, h2))
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

            // Power menu clicks
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (power.menu_open()) {
                    int w2, h2;
                    SDL_GetWindowSize(window, &w2, &h2);
                    if (power.handle_click(event.button.x, event.button.y, w2, h2))
                        continue;
                }
            }

            // System tray clicks
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int w2, h2;
                SDL_GetWindowSize(window, &w2, &h2);
                if (systray.handle_click(event.button.x, event.button.y, w2, 36)) {
                    // Delegate power icon to power manager
                    if (systray.active_panel() == 0) {
                        systray.clear_panel();
                        power.toggle_menu();
                    }
                    continue;
                }
            }

            // System tray hover
            if (event.type == SDL_MOUSEMOTION) {
                int w2, h2;
                SDL_GetWindowSize(window, &w2, &h2);
                systray.on_mouse_move(event.motion.x, event.motion.y, w2, 36);
            }

            // Dock + sidebar clicks — check before WM so they take priority
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (handle_dock_click(event.button.x, event.button.y, w, h,
                                      registry, pm, wm)) {
                    continue;
                }
                // sidebar removed
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

        // Track new windows in workspaces
        for (auto& win : wm.windows()) {
            bool tracked = false;
            for (int i = 0; i < workspaces.count(); i++) {
                if (workspaces.workspace(i).window_ids.count(win.id)) {
                    tracked = true;
                    break;
                }
            }
            if (!tracked) {
                workspaces.assign_window(win.id);
            }
        }

        // Notification manager: expire old toasts
        notifications.tick(SDL_GetTicks());

        // Network manager: periodic updates
        network.tick(SDL_GetTicks());

        // Power manager: idle tracking, screen dim
        power.tick(SDL_GetTicks());
        if (power.should_quit()) running = false;

        // Animation manager
        animations.tick(SDL_GetTicks());
        // Clean up completed close animations
        auto closed = animations.pop_completed_closes();
        for (int wid : closed) {
            // Already visually gone — nothing extra needed
            (void)wid;
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
        RenderCtx ctx = {renderer, &frost, &fonts, w, h, &theme_mgr, &animations};
        render_topbar(ctx);
        // render_left_sidebar removed
        render_right_sidebar(ctx);
        wm.render(ctx);
        render_dock(ctx, wm, registry);

        // System tray icons (rendered over topbar)
        systray.render(renderer, &fonts, w, 36);

        // Render context menu
        ctx_menu.render(renderer, &fonts);

        // Render system tray panels (volume, network, battery, notifications)
        systray.render_panels(renderer, &fonts, w);

        // Render toast notifications on top of everything
        notifications.render(renderer, &fonts, w);

        // Power menu
        power.render_menu(renderer, &frost, &fonts, w, h);

        // Workspace indicator (above dock)
        workspaces.render_indicator(renderer, &fonts, w / 2, h - 12);

        // Workspace switcher overlay
        workspaces.render_switcher(renderer, &fonts, wm, w, h);

        // File association dialog
        file_assoc.render_dialog(renderer, &fonts, w, h);

        // App launcher overlay
        launcher.render(renderer, &frost, &fonts, w, h);

        // Screen dim from idle
        power.render_dim(renderer, w, h);

        // Lock screen renders on top of ALL UI
        lockscreen.render(renderer, &frost, &fonts, w, h, wallpaper);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Destroy all windows (and their AppContent objects) BEFORE unloading
    // the dynamic .so plugins — the AppContent vtables live in the .so code.
    wm.close_all_windows();

    // Now safe to unload dynamic app plugins
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
