#include "frost.h"
#include <algorithm>

bool FrostRenderer::init(SDL_Renderer* r, int w, int h) {
    w_ = w; h_ = h;

    scene_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, w, h);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    blur_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, w / SCALE + 1, h / SCALE + 1);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    return scene_ && blur_;
}

void FrostRenderer::cleanup() {
    if (scene_) { SDL_DestroyTexture(scene_); scene_ = nullptr; }
    if (blur_)  { SDL_DestroyTexture(blur_);  blur_  = nullptr; }
}

void FrostRenderer::resize(SDL_Renderer* r, int w, int h) {
    if (w == w_ && h == h_) return;
    cleanup();
    init(r, w, h);
}

void FrostRenderer::process_blur(SDL_Renderer* r) {
    SDL_SetRenderTarget(r, blur_);
    SDL_RenderCopy(r, scene_, nullptr, nullptr);
}

void FrostRenderer::render_scene(SDL_Renderer* r) {
    SDL_SetRenderTarget(r, nullptr);
    SDL_RenderCopy(r, scene_, nullptr, nullptr);
}

void FrostRenderer::render_panel(SDL_Renderer* r, SDL_Rect rect,
                                  SDL_Color tint) {
    int bw = w_ / SCALE + 1;
    int bh = h_ / SCALE + 1;

    SDL_Rect src = {
        std::max(0, rect.x / SCALE),
        std::max(0, rect.y / SCALE),
        std::min(rect.w / SCALE + 1, bw - std::max(0, rect.x / SCALE)),
        std::min(rect.h / SCALE + 1, bh - std::max(0, rect.y / SCALE))
    };

    SDL_SetTextureBlendMode(blur_, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(r, blur_, &src, &rect);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, tint.r, tint.g, tint.b, tint.a);
    SDL_RenderFillRect(r, &rect);
}
