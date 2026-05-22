#pragma once
#include <SDL2/SDL.h>

class FrostRenderer {
public:
    bool init(SDL_Renderer* r, int w, int h);
    void cleanup();
    void resize(SDL_Renderer* r, int w, int h);

    SDL_Texture* scene_target() { return scene_; }
    void process_blur(SDL_Renderer* r);
    void render_scene(SDL_Renderer* r);
    void render_panel(SDL_Renderer* r, SDL_Rect rect,
                      SDL_Color tint = {15, 20, 35, 140});

private:
    SDL_Texture* scene_ = nullptr;
    SDL_Texture* blur_ = nullptr;
    int w_ = 0, h_ = 0;
    static const int SCALE = 8;
};
