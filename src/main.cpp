#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <ctime>

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;

struct Clock {
    TTF_Font* font;
    TTF_Font* date_font;
};

static std::string get_time_str() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[16];
    int hour = t->tm_hour % 12;
    if (hour == 0) hour = 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s",
             hour, t->tm_min, t->tm_hour >= 12 ? "PM" : "AM");
    return buf;
}

static std::string get_date_str() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    static const char* days[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char* months[] = {"January","February","March","April","May","June",
                                   "July","August","September","October","November","December"};
    char buf[64];
    snprintf(buf, sizeof(buf), "%s, %s %d", days[t->tm_wday], months[t->tm_mon], t->tm_mday);
    return buf;
}

static void render_text_centered(SDL_Renderer* renderer, TTF_Font* font,
                                  const char* text, int center_x, int y,
                                  SDL_Color color) {
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = { center_x - surf->w / 2, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_taskbar(SDL_Renderer* renderer, TTF_Font* font, int screen_w, int screen_h) {
    int bar_h = 40;
    int bar_y = screen_h - bar_h;

    // Taskbar background - dark translucent bar
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 200);
    SDL_Rect bar = { 0, bar_y, screen_w, bar_h };
    SDL_RenderFillRect(renderer, &bar);

    // Subtle top border
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 150);
    SDL_RenderDrawLine(renderer, 0, bar_y, screen_w, bar_y);

    // "Start" button area on the left
    SDL_Color white = {220, 220, 230, 255};
    SDL_Surface* surf = TTF_RenderText_Blended(font, ":: HerOS", white);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst = { 12, bar_y + (bar_h - surf->h) / 2, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }

    // Clock on the right side of taskbar
    std::string time_str = get_time_str();
    surf = TTF_RenderText_Blended(font, time_str.c_str(), white);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst = { screen_w - surf->w - 12, bar_y + (bar_h - surf->h) / 2, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
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

    SDL_Window* window = SDL_CreateWindow(
        "HerOS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load fonts
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    TTF_Font* clock_font = TTF_OpenFont(font_path, 64);
    TTF_Font* date_font  = TTF_OpenFont(font_path, 22);
    TTF_Font* bar_font   = TTF_OpenFont(font_path, 15);
    if (!clock_font || !date_font || !bar_font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // Desktop gradient background - dark blue
        for (int y = 0; y < h; y++) {
            float t = static_cast<float>(y) / h;
            Uint8 r = static_cast<Uint8>(10 + t * 15);
            Uint8 g = static_cast<Uint8>(15 + t * 25);
            Uint8 b = static_cast<Uint8>(40 + t * 50);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_RenderDrawLine(renderer, 0, y, w, y);
        }

        // Centered clock and date
        SDL_Color white = {230, 230, 240, 255};
        SDL_Color dim   = {160, 165, 185, 255};

        std::string time_str = get_time_str();
        std::string date_str = get_date_str();

        render_text_centered(renderer, clock_font, time_str.c_str(), w / 2, h / 2 - 60, white);
        render_text_centered(renderer, date_font, date_str.c_str(), w / 2, h / 2 + 20, dim);

        // Taskbar
        draw_taskbar(renderer, bar_font, w, h);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60fps
    }

    TTF_CloseFont(clock_font);
    TTF_CloseFont(date_font);
    TTF_CloseFont(bar_font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
