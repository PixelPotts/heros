#include "animation.h"

static WindowAnim anims[MAX_ANIMATIONS];

void anim_init(void)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++)
        anims[i].active = 0;
}

int ease_apply(EaseType type, int t)
{
    /* t in [0, 256], returns [0, 256] */
    switch (type) {
    case EASE_LINEAR:
        return t;

    case EASE_QUAD_IN:
        return (t * t) / 256;

    case EASE_QUAD_OUT: {
        int inv = 256 - t;
        return 256 - (inv * inv) / 256;
    }

    case EASE_QUAD_INOUT:
        if (t < 128)
            return (t * t) / 128;
        else {
            int inv = 256 - t;
            return 256 - (inv * inv) / 128;
        }

    case EASE_CUBIC_OUT: {
        int inv = 256 - t;
        return 256 - (inv * inv * inv) / (256 * 256);
    }

    case EASE_ELASTIC_OUT: {
        if (t >= 256) return 256;
        if (t <= 0) return 0;
        /* Simplified elastic: overshoot then settle */
        int base = ease_apply(EASE_CUBIC_OUT, t);
        int bounce = hal_sin256(t * 3 / 4);
        int decay = (256 - t);
        return base + (bounce * decay) / (256 * 8);
    }

    case EASE_BOUNCE_OUT: {
        if (t < 92) return (t * t * 256) / (92 * 92);
        if (t < 184) {
            int v = t - 138;
            return 192 + (v * v * 64) / (46 * 46);
        }
        int v = t - 230;
        return 240 + (v * v * 16) / (26 * 26);
    }
    }
    return t;
}

int anim_start(int window_id, EaseType ease, uint32_t duration_ms,
                Rect from, Rect to, int fade_from, int fade_to)
{
    /* Find free slot or replace existing for this window */
    int slot = -1;
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (anims[i].active && anims[i].window_id == window_id) {
            slot = i; break;
        }
    }
    if (slot < 0) {
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (!anims[i].active) { slot = i; break; }
        }
    }
    if (slot < 0) return -1;

    anims[slot].active = 1;
    anims[slot].window_id = window_id;
    anims[slot].start_time = hal_get_ticks();
    anims[slot].duration = duration_ms;
    anims[slot].ease = ease;
    anims[slot].from_x = from.x;
    anims[slot].from_y = from.y;
    anims[slot].from_w = from.w;
    anims[slot].from_h = from.h;
    anims[slot].to_x = to.x;
    anims[slot].to_y = to.y;
    anims[slot].to_w = to.w;
    anims[slot].to_h = to.h;
    anims[slot].fade_from = fade_from;
    anims[slot].fade_to = fade_to;
    return slot;
}

int anim_tick(int window_id, Rect *out_rect, int *out_alpha)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (!anims[i].active || anims[i].window_id != window_id)
            continue;

        uint32_t elapsed = hal_get_ticks() - anims[i].start_time;
        if (elapsed >= anims[i].duration) {
            /* Animation complete */
            out_rect->x = anims[i].to_x;
            out_rect->y = anims[i].to_y;
            out_rect->w = anims[i].to_w;
            out_rect->h = anims[i].to_h;
            *out_alpha = anims[i].fade_to;
            anims[i].active = 0;
            return 0;  /* finished */
        }

        int t = (int)(elapsed * 256 / anims[i].duration);
        int e = ease_apply(anims[i].ease, t);

        out_rect->x = anims[i].from_x + (anims[i].to_x - anims[i].from_x) * e / 256;
        out_rect->y = anims[i].from_y + (anims[i].to_y - anims[i].from_y) * e / 256;
        out_rect->w = anims[i].from_w + (anims[i].to_w - anims[i].from_w) * e / 256;
        out_rect->h = anims[i].from_h + (anims[i].to_h - anims[i].from_h) * e / 256;
        *out_alpha = anims[i].fade_from + (anims[i].fade_to - anims[i].fade_from) * e / 256;
        return 1;  /* still animating */
    }

    return -1;  /* no animation found */
}

void anim_cancel(int window_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (anims[i].active && anims[i].window_id == window_id)
            anims[i].active = 0;
    }
}

int anim_is_active(int window_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (anims[i].active && anims[i].window_id == window_id)
            return 1;
    }
    return 0;
}
