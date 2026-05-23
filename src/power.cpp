#include "power.h"
#include "audio.h"
#include "lockscreen.h"
#include <cstdio>
#include <cmath>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};
static const SDL_Color RED    = {255, 100, 100, 220};

const PowerManager::MenuItem PowerManager::MENU_ITEMS[] = {
    {"Shut Down",  "Turn off HerOS",        Icon::Power,  PowerAction::Shutdown},
    {"Restart",    "Restart the system",     Icon::Ring,   PowerAction::Restart},
    {"Sleep",      "Suspend to save power",  Icon::Moon,   PowerAction::Sleep},
    {"Log Out",    "End current session",    Icon::People, PowerAction::Logout},
    {"Lock",       "Lock the screen",        Icon::Lock,   PowerAction::Lock},
};
const int PowerManager::MENU_ITEM_COUNT = 5;

// ── Init ────────────────────────────────────────────────────────

void PowerManager::init(AudioManager* audio, LockScreen* lockscreen) {
    audio_ = audio;
    lockscreen_ = lockscreen;
    last_activity_ = SDL_GetTicks();
}

void PowerManager::open_menu() {
    menu_open_ = true;
    hover_item_ = -1;
}

// ── Idle tracking ───────────────────────────────────────────────

void PowerManager::tick(uint32_t now) {
    uint32_t idle = now - last_activity_;

    // Screen dimming
    if (idle > dim_timeout_) {
        dimming_ = true;
        float progress = (float)(idle - dim_timeout_) / 10000.0f; // fade over 10s
        dim_alpha_ = std::min(0.7f, progress);
    } else {
        dimming_ = false;
        dim_alpha_ = 0;
    }

    // Auto-lock after longer idle
    if (idle > lock_timeout_ && lockscreen_ && !lockscreen_->is_locked()) {
        if (on_lock_) on_lock_();
    }
}

// ── Execute power action ────────────────────────────────────────

void PowerManager::execute(PowerAction action) {
    switch (action) {
    case PowerAction::Shutdown:
        fprintf(stderr, "PowerManager: Shutting down...\n");
        if (audio_) audio_->play(SystemSound::Alert);
        should_quit_ = true;
        break;
    case PowerAction::Restart:
        fprintf(stderr, "PowerManager: Restarting...\n");
        if (audio_) audio_->play(SystemSound::Alert);
        should_restart_ = true;
        should_quit_ = true;
        break;
    case PowerAction::Sleep:
        fprintf(stderr, "PowerManager: Sleeping...\n");
        if (audio_) audio_->play(SystemSound::Lock);
        sleeping_ = true;
        dimming_ = true;
        dim_alpha_ = 0.9f;
        if (on_lock_) on_lock_();
        break;
    case PowerAction::Logout:
        fprintf(stderr, "PowerManager: Logging out...\n");
        if (on_lock_) on_lock_();
        break;
    case PowerAction::Lock:
        if (on_lock_) on_lock_();
        break;
    case PowerAction::None:
        break;
    }
    menu_open_ = false;
}

// ── Click handling ──────────────────────────────────────────────

bool PowerManager::handle_click(int mx, int my, int screen_w, int screen_h) {
    if (!menu_open_) return false;

    int menu_w = 280, menu_h = MENU_ITEM_COUNT * 56 + 60;
    int menu_x = (screen_w - menu_w) / 2;
    int menu_y = (screen_h - menu_h) / 2;

    // Outside menu
    if (mx < menu_x || mx >= menu_x + menu_w || my < menu_y || my >= menu_y + menu_h) {
        menu_open_ = false;
        return true;
    }

    // Check items
    int item_y = menu_y + 50;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (my >= item_y && my < item_y + 52) {
            execute(MENU_ITEMS[i].action);
            return true;
        }
        item_y += 56;
    }

    return true;
}

// ── Render menu ─────────────────────────────────────────────────

void PowerManager::render_menu(SDL_Renderer* r, FrostRenderer* frost, const Fonts* fonts,
                                int screen_w, int screen_h) {
    if (!menu_open_) return;

    // Dark overlay
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 15, 160);
    SDL_Rect full = {0, 0, screen_w, screen_h};
    SDL_RenderFillRect(r, &full);

    int menu_w = 280, menu_h = MENU_ITEM_COUNT * 56 + 60;
    int menu_x = (screen_w - menu_w) / 2;
    int menu_y = (screen_h - menu_h) / 2;

    // Panel
    SDL_Rect panel = {menu_x, menu_y, menu_w, menu_h};
    if (frost) frost->render_panel(r, panel, {10, 14, 25, 200});
    draw::rounded_rect(r, panel, 12, {180, 195, 220, 40});

    // Title
    draw::text_centered(r, fonts->title, "Power", screen_w / 2, menu_y + 16, WHITE);
    draw::line(r, menu_x + 12, menu_y + 42, menu_x + menu_w - 12, menu_y + 42, {180, 195, 220, 30});

    // Items
    int item_y = menu_y + 50;
    int mx_now, my_now;
    SDL_GetMouseState(&mx_now, &my_now);

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        SDL_Rect item = {menu_x + 8, item_y, menu_w - 16, 52};

        // Hover detection
        bool hovered = (mx_now >= item.x && mx_now < item.x + item.w &&
                        my_now >= item.y && my_now < item.y + item.h);

        if (hovered) {
            draw::filled_rounded_rect(r, item, 8, {100, 150, 255, 25});
        }

        // Icon
        SDL_Color ic = (MENU_ITEMS[i].action == PowerAction::Shutdown) ? RED : DIM;
        if (hovered) ic = ACCENT;
        draw::icon(r, MENU_ITEMS[i].icon, menu_x + 34, item_y + 26, 20, ic);

        // Label
        SDL_Color lc = (MENU_ITEMS[i].action == PowerAction::Shutdown) ? RED : WHITE;
        if (hovered) lc = WHITE;
        draw::text(r, fonts->body, MENU_ITEMS[i].label, menu_x + 56, item_y + 10, lc);

        // Description
        draw::text(r, fonts->small, MENU_ITEMS[i].desc, menu_x + 56, item_y + 30, DIM);

        item_y += 56;
    }
}

// ── Render dim overlay ──────────────────────────────────────────

void PowerManager::render_dim(SDL_Renderer* r, int screen_w, int screen_h) {
    if (dim_alpha_ <= 0) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    Uint8 alpha = (Uint8)(dim_alpha_ * 255);
    SDL_SetRenderDrawColor(r, 0, 0, 5, alpha);
    SDL_Rect full = {0, 0, screen_w, screen_h};
    SDL_RenderFillRect(r, &full);
}
