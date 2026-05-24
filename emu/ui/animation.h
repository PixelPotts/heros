#ifndef UI_ANIMATION_H
#define UI_ANIMATION_H

#include "types.h"

#define MAX_ANIMATIONS  16

typedef enum {
    EASE_LINEAR,
    EASE_QUAD_IN,
    EASE_QUAD_OUT,
    EASE_QUAD_INOUT,
    EASE_CUBIC_OUT,
    EASE_ELASTIC_OUT,
    EASE_BOUNCE_OUT
} EaseType;

typedef struct {
    int      active;
    int      window_id;
    uint32_t start_time;
    uint32_t duration;
    EaseType ease;
    int      from_x, from_y, from_w, from_h;
    int      to_x, to_y, to_w, to_h;
    int      fade_from, fade_to;  /* 0-255 opacity */
} WindowAnim;

void  anim_init(void);
int   anim_start(int window_id, EaseType ease, uint32_t duration_ms,
                  Rect from, Rect to, int fade_from, int fade_to);
int   anim_tick(int window_id, Rect *out_rect, int *out_alpha);
void  anim_cancel(int window_id);
int   anim_is_active(int window_id);

/* Easing functions: t in [0, 256], returns [0, 256] */
int ease_apply(EaseType type, int t);

#endif
