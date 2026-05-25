/* doom3d.c — Wolfenstein 3D-style raycasting FPS for HerOS
 * 16×16 map, DDA raycasting, flat-shaded walls, static enemies, hitscan shooting
 */

#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../window.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"
#include "../../kernel/kprintf.h"

/* ── Fixed-point (16.16) helpers ──────────────────────────────── */
#define FP_SHIFT   16
#define FP_ONE     (1 << FP_SHIFT)
#define FP_HALF    (FP_ONE >> 1)
#define INT_TO_FP(x) ((x) << FP_SHIFT)
#define FP_TO_INT(x) ((x) >> FP_SHIFT)
#define FP_FRAC(x)   ((x) & (FP_ONE - 1))

/* ── Map ─────────────────────────────────────────────────────── */
#define MAP_W 16
#define MAP_H 16

/* Wall types: 0=empty, 1=gray, 2=red, 3=blue, 4=green */
static const uint8_t level_map[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,3,3,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,3,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,4,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,0,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,4,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,3,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,3,3,0,0,0,0,0,2,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

/* ── Enemies ─────────────────────────────────────────────────── */
#define MAX_ENEMIES 6

typedef struct {
    int  x, y;        /* fixed-point position */
    int  alive;
    int  hit_timer;   /* frames of flash remaining */
} Enemy;

static const int enemy_start_x[MAX_ENEMIES] = { 5, 10, 7, 13, 3, 11 };
static const int enemy_start_y[MAX_ENEMIES] = { 5, 3, 7, 11, 12, 7 };

/* ── Rendering constants ──────────────────────────────────────── */
#define STRIP_W     3       /* pixels per ray column */
#define MAX_RAYS    200     /* max number of rays */
#define FOV_ANGLES  64      /* field of view in 256-angle units (~90 degrees) */
#define MAX_DIST    (20 * 256)  /* max ray distance in Q8 */

/* ── Game state ──────────────────────────────────────────────── */
typedef struct {
    /* Player */
    int  px, py;        /* position (Q16 fixed-point) */
    int  angle;         /* 0-255 (256 = full circle) */
    int  health;
    int  ammo;
    int  score;

    /* Enemies */
    Enemy enemies[MAX_ENEMIES];

    /* Per-column depth buffer for occlusion */
    int  depth_buf[MAX_RAYS];

    /* Rendering area */
    int  view_w, view_h;
    int  num_rays;

    /* Shoot flash */
    int  shoot_flash;

    /* Game over */
    int  game_over;
} DoomState;

/* ── Wall colors by type ─────────────────────────────────────── */
static Color wall_color(int type, int side, int shade)
{
    int r, g, b;
    switch (type) {
    case 1: r = 160; g = 160; b = 160; break;  /* gray stone */
    case 2: r = 180; g = 60;  b = 50;  break;  /* red brick */
    case 3: r = 60;  g = 80;  b = 180; break;  /* blue */
    case 4: r = 50;  g = 160; b = 60;  break;  /* green */
    default: r = 128; g = 128; b = 128; break;
    }
    /* Darken side hits for depth perception */
    if (side) { r = r * 3 / 4; g = g * 3 / 4; b = b * 3 / 4; }
    /* Distance shading */
    r = r * shade / 256;
    g = g * shade / 256;
    b = b * shade / 256;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

/* ── DDA raycasting ──────────────────────────────────────────── */
/* Returns wall distance in Q8 fixed-point, sets *wall_type and *side */
static int cast_ray(DoomState *ds, int ray_angle, int *wall_type, int *side_out)
{
    ray_angle = ray_angle & 255;
    /* Direction vector in Q8 */
    int dir_x = hal_cos256(ray_angle);  /* Q8 */
    int dir_y = hal_sin256(ray_angle);  /* Q8 */

    /* Player position in map coords (Q8 from Q16) */
    int pos_x = ds->px >> 8;  /* Q8 */
    int pos_y = ds->py >> 8;  /* Q8 */

    int map_x = pos_x >> 8;   /* integer map cell */
    int map_y = pos_y >> 8;

    /* Step direction */
    int step_x = (dir_x >= 0) ? 1 : -1;
    int step_y = (dir_y >= 0) ? 1 : -1;

    /* Distance to next grid line in each axis (Q8) */
    int side_dist_x, side_dist_y;
    int delta_dist_x, delta_dist_y;

    /* Avoid division by zero */
    if (dir_x == 0) dir_x = 1;
    if (dir_y == 0) dir_y = 1;

    /* delta_dist = |1/dir| * 256 in Q8 */
    int abs_dx = dir_x < 0 ? -dir_x : dir_x;
    int abs_dy = dir_y < 0 ? -dir_y : dir_y;
    delta_dist_x = (256 * 256) / abs_dx;  /* Q8 */
    delta_dist_y = (256 * 256) / abs_dy;  /* Q8 */

    /* Initial side distances */
    if (dir_x > 0) {
        side_dist_x = ((((map_x + 1) << 8) - pos_x) * delta_dist_x) >> 8;
    } else {
        side_dist_x = ((pos_x - (map_x << 8)) * delta_dist_x) >> 8;
    }
    if (dir_y > 0) {
        side_dist_y = ((((map_y + 1) << 8) - pos_y) * delta_dist_y) >> 8;
    } else {
        side_dist_y = ((pos_y - (map_y << 8)) * delta_dist_y) >> 8;
    }

    /* DDA loop */
    int hit = 0, side = 0;
    for (int i = 0; i < 64; i++) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            side = 0;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            side = 1;
        }
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) break;
        if (level_map[map_y][map_x] != 0) {
            hit = 1;
            break;
        }
    }

    *side_out = side;
    if (!hit) { *wall_type = 0; return MAX_DIST; }
    *wall_type = level_map[map_y][map_x];

    /* Perpendicular wall distance (avoids fisheye) */
    int perp_dist;
    if (side == 0) {
        perp_dist = side_dist_x - delta_dist_x;
    } else {
        perp_dist = side_dist_y - delta_dist_y;
    }
    if (perp_dist < 1) perp_dist = 1;
    return perp_dist;
}

/* ── Render ──────────────────────────────────────────────────── */
static void doom3d_render(AppContent *self, Rect cr)
{
    DoomState *ds = (DoomState *)self->data;
    ds->view_w = cr.w;
    ds->view_h = cr.h;
    ds->num_rays = cr.w / STRIP_W;
    if (ds->num_rays > MAX_RAYS) ds->num_rays = MAX_RAYS;

    int half_h = cr.h / 2;

    /* ── Ceiling & floor ─────────────────────────────────────── */
    draw_filled_rect(RECT(cr.x, cr.y, cr.w, half_h), RGB(30, 30, 60));
    draw_filled_rect(RECT(cr.x, cr.y + half_h, cr.w, cr.h - half_h), RGB(50, 50, 50));

    /* ── Shoot flash overlay ─────────────────────────────────── */
    if (ds->shoot_flash > 0) {
        draw_filled_rect(RECT(cr.x, cr.y, cr.w, cr.h), RGB(255, 200, 100));
        ds->shoot_flash--;
    }

    /* ── Raycasting ──────────────────────────────────────────── */
    int start_angle = ds->angle - FOV_ANGLES / 2;

    for (int col = 0; col < ds->num_rays; col++) {
        /* Linear interpolation of angle across FOV */
        int ray_angle = start_angle + (col * FOV_ANGLES) / ds->num_rays;
        ray_angle = ray_angle & 255;

        int wtype, side;
        int dist = cast_ray(ds, ray_angle, &wtype, &side);

        ds->depth_buf[col] = dist;

        if (wtype == 0) continue;

        /* Wall strip height — proportional to 1/distance */
        /* view_h * 256 / dist gives strip height (dist is Q8) */
        int strip_h = (cr.h * 200) / dist;
        if (strip_h > cr.h) strip_h = cr.h;
        if (strip_h < 1) strip_h = 1;

        int strip_top = cr.y + half_h - strip_h / 2;
        int strip_x = cr.x + col * STRIP_W;

        /* Distance shading: closer = brighter */
        int shade = 256 - (dist * 200 / MAX_DIST);
        if (shade < 40) shade = 40;
        if (shade > 256) shade = 256;

        Color wc = wall_color(wtype, side, shade);
        draw_filled_rect(RECT(strip_x, strip_top, STRIP_W, strip_h), wc);
    }

    /* ── Render enemies ──────────────────────────────────────── */
    /* Sort enemies by distance (back-to-front) via simple selection sort */
    int order[MAX_ENEMIES];
    int edist[MAX_ENEMIES];
    for (int i = 0; i < MAX_ENEMIES; i++) {
        order[i] = i;
        int dx = ds->enemies[i].x - ds->px;
        int dy = ds->enemies[i].y - ds->py;
        /* Approximate distance (Q16 -> rough magnitude) */
        dx >>= 8; dy >>= 8;  /* now Q8 */
        edist[i] = (dx * dx + dy * dy); /* Q16-ish for sorting */
    }
    /* Sort descending (farthest first) */
    for (int i = 0; i < MAX_ENEMIES - 1; i++) {
        for (int j = i + 1; j < MAX_ENEMIES; j++) {
            if (edist[order[j]] > edist[order[i]]) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }

    int player_cos = hal_cos256(ds->angle);
    int player_sin = hal_sin256(ds->angle);

    for (int idx = 0; idx < MAX_ENEMIES; idx++) {
        Enemy *e = &ds->enemies[order[idx]];
        if (!e->alive && e->hit_timer <= 0) continue;

        /* Relative position (Q16 -> Q8) */
        int rel_x = (e->x - ds->px) >> 8;
        int rel_y = (e->y - ds->py) >> 8;

        /* Transform to camera space (Q8 * Q8 = Q16, shift back to Q8) */
        int cam_x = (rel_x * player_cos + rel_y * player_sin) >> 8;
        int cam_y = (-rel_x * player_sin + rel_y * player_cos) >> 8;

        if (cam_y < 10) continue;  /* Behind or too close */

        /* Screen X position */
        int screen_x = cr.x + cr.w / 2 + (cam_x * cr.h) / cam_y;

        /* Sprite height/width */
        int spr_h = (cr.h * 180) / cam_y;
        if (spr_h > cr.h) spr_h = cr.h;
        int spr_w = spr_h / 3;
        if (spr_w < 4) spr_w = 4;

        int spr_top = cr.y + half_h - spr_h / 2;
        int spr_left = screen_x - spr_w / 2;

        /* Check depth buffer for occlusion (per-column) */
        int col_start = (spr_left - cr.x) / STRIP_W;
        int col_end = (spr_left + spr_w - cr.x) / STRIP_W;
        if (col_start < 0) col_start = 0;
        if (col_end >= ds->num_rays) col_end = ds->num_rays - 1;

        /* Enemy distance in Q8 for depth comparison */
        int e_dist_q8 = cam_y;

        Color ec;
        if (e->hit_timer > 0) {
            ec = RGB(255, 60, 60);  /* flash red when hit */
            e->hit_timer--;
            if (!e->alive && e->hit_timer <= 0) continue;
        } else {
            ec = RGB(200, 50, 200);  /* purple enemies */
        }

        /* Draw enemy as vertical bar, checking depth per strip */
        for (int c = col_start; c <= col_end; c++) {
            if (e_dist_q8 < ds->depth_buf[c]) {
                int sx = cr.x + c * STRIP_W;
                int sw = STRIP_W;
                if (sx < spr_left) { sw -= (spr_left - sx); sx = spr_left; }
                if (sx + sw > spr_left + spr_w) sw = spr_left + spr_w - sx;
                if (sw > 0) {
                    /* Draw body */
                    draw_filled_rect(RECT(sx, spr_top + spr_h / 5, sw, spr_h * 3 / 5), ec);
                    /* Draw head */
                    int head_h = spr_h / 5;
                    int head_w = sw;
                    draw_filled_rect(RECT(sx, spr_top, head_w, head_h), RGB(220, 180, 140));
                    /* Draw eyes */
                    if (head_h > 4 && head_w > 4) {
                        int eye_y = spr_top + head_h / 3;
                        draw_filled_rect(RECT(sx + 1, eye_y, 2, 2), RGB(255, 0, 0));
                    }
                }
            }
        }
    }

    /* ── Crosshair ───────────────────────────────────────────── */
    int cx = cr.x + cr.w / 2;
    int cy = cr.y + cr.h / 2;
    draw_filled_rect(RECT(cx - 8, cy, 17, 1), RGB(0, 255, 0));
    draw_filled_rect(RECT(cx, cy - 8, 1, 17), RGB(0, 255, 0));

    /* ── HUD ─────────────────────────────────────────────────── */
    int hud_y = cr.y + cr.h - 24;
    /* Background bar */
    draw_filled_rect(RECT(cr.x, hud_y, cr.w, 24), RGB(20, 20, 20));

    /* Health bar */
    int hp_w = ds->health * 100 / 100;
    int hp_r = ds->health > 50 ? 0 : (ds->health > 25 ? 200 : 255);
    int hp_g = ds->health > 50 ? 200 : (ds->health > 25 ? 200 : 60);
    draw_filled_rect(RECT(cr.x + 4, hud_y + 4, 100, 16), RGB(60, 60, 60));
    draw_filled_rect(RECT(cr.x + 4, hud_y + 4, hp_w, 16), RGB(hp_r, hp_g, 0));
    draw_text(cr.x + 8, hud_y + 6, "HP", COLOR_WHITE, FONT_SIZE_SMALL);

    /* Ammo */
    char buf[32];
    int blen = 0;
    buf[blen++] = 'A'; buf[blen++] = 'M'; buf[blen++] = 'M'; buf[blen++] = 'O';
    buf[blen++] = ':';
    /* int to string for ammo */
    int a = ds->ammo;
    if (a >= 100) { buf[blen++] = '0' + a / 100; a %= 100; buf[blen++] = '0' + a / 10; a %= 10; }
    else if (a >= 10) { buf[blen++] = '0' + a / 10; a %= 10; }
    buf[blen++] = '0' + a;
    buf[blen] = '\0';
    draw_text(cr.x + 120, hud_y + 6, buf, RGB(200, 200, 50), FONT_SIZE_SMALL);

    /* Score */
    blen = 0;
    buf[blen++] = 'S'; buf[blen++] = 'C'; buf[blen++] = 'O'; buf[blen++] = 'R';
    buf[blen++] = 'E'; buf[blen++] = ':';
    int s = ds->score;
    if (s >= 100) { buf[blen++] = '0' + s / 100; s %= 100; buf[blen++] = '0' + s / 10; s %= 10; }
    else if (s >= 10) { buf[blen++] = '0' + s / 10; s %= 10; }
    buf[blen++] = '0' + s;
    buf[blen] = '\0';
    draw_text(cr.x + 220, hud_y + 6, buf, COLOR_WHITE, FONT_SIZE_SMALL);

    /* ── Mini-map (top-right corner, 64×64) ──────────────────── */
    int mm_size = 64;
    int mm_x = cr.x + cr.w - mm_size - 4;
    int mm_y = cr.y + 4;
    int cell = mm_size / MAP_W;  /* 4px per cell */

    /* Map background */
    draw_filled_rect(RECT(mm_x, mm_y, mm_size, mm_size), COLOR(0, 0, 0, 180));

    /* Walls */
    for (int my = 0; my < MAP_H; my++) {
        for (int mx = 0; mx < MAP_W; mx++) {
            if (level_map[my][mx] != 0) {
                Color mc;
                switch (level_map[my][mx]) {
                case 1: mc = RGB(120, 120, 120); break;
                case 2: mc = RGB(160, 50, 40); break;
                case 3: mc = RGB(40, 60, 160); break;
                case 4: mc = RGB(40, 130, 50); break;
                default: mc = RGB(100, 100, 100); break;
                }
                draw_filled_rect(RECT(mm_x + mx * cell, mm_y + my * cell, cell, cell), mc);
            }
        }
    }

    /* Player dot on minimap */
    int pp_x = mm_x + FP_TO_INT(ds->px) * cell;
    int pp_y = mm_y + FP_TO_INT(ds->py) * cell;
    draw_filled_rect(RECT(pp_x - 1, pp_y - 1, 3, 3), RGB(0, 255, 0));

    /* Player direction line */
    int dir_len = 6;
    int dx2 = pp_x + (hal_cos256(ds->angle) * dir_len) / 256;
    int dy2 = pp_y + (hal_sin256(ds->angle) * dir_len) / 256;
    draw_line(pp_x, pp_y, dx2, dy2, RGB(0, 255, 0));

    /* Enemy dots on minimap */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (ds->enemies[i].alive) {
            int ex = mm_x + FP_TO_INT(ds->enemies[i].x) * cell;
            int ey = mm_y + FP_TO_INT(ds->enemies[i].y) * cell;
            draw_filled_rect(RECT(ex, ey, 2, 2), RGB(255, 0, 0));
        }
    }

    /* ── Game over overlay ───────────────────────────────────── */
    if (ds->game_over) {
        draw_filled_rect(RECT(cr.x, cr.y + cr.h / 2 - 20, cr.w, 40), RGB(0, 0, 0));
        if (ds->score >= MAX_ENEMIES) {
            draw_text_centered(cr.x + cr.w / 2, cr.y + cr.h / 2 - 8,
                "YOU WIN!", RGB(0, 255, 0), FONT_SIZE_LARGE);
        } else {
            draw_text_centered(cr.x + cr.w / 2, cr.y + cr.h / 2 - 8,
                "GAME OVER", RGB(255, 0, 0), FONT_SIZE_LARGE);
        }
    }
}

/* ── Collision check ─────────────────────────────────────────── */
static int is_wall(int fx, int fy)
{
    int mx = FP_TO_INT(fx);
    int my = FP_TO_INT(fy);
    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) return 1;
    return level_map[my][mx] != 0;
}

/* ── Try to move with wall sliding ───────────────────────────── */
static void try_move(DoomState *ds, int dx, int dy)
{
    int margin = FP_ONE / 4;  /* 0.25 cell wall margin */
    int nx = ds->px + dx;
    int ny = ds->py + dy;

    /* Try full move */
    if (!is_wall(nx + margin, ny + margin) &&
        !is_wall(nx - margin, ny + margin) &&
        !is_wall(nx + margin, ny - margin) &&
        !is_wall(nx - margin, ny - margin)) {
        ds->px = nx;
        ds->py = ny;
        return;
    }
    /* Try X-only slide */
    if (!is_wall(ds->px + dx + margin, ds->py + margin) &&
        !is_wall(ds->px + dx - margin, ds->py + margin) &&
        !is_wall(ds->px + dx + margin, ds->py - margin) &&
        !is_wall(ds->px + dx - margin, ds->py - margin)) {
        ds->px += dx;
        return;
    }
    /* Try Y-only slide */
    if (!is_wall(ds->px + margin, ds->py + dy + margin) &&
        !is_wall(ds->px - margin, ds->py + dy + margin) &&
        !is_wall(ds->px + margin, ds->py + dy - margin) &&
        !is_wall(ds->px - margin, ds->py + dy - margin)) {
        ds->py += dy;
    }
}

/* ── Shoot (hitscan toward center of view) ───────────────────── */
static void do_shoot(DoomState *ds)
{
    if (ds->ammo <= 0 || ds->game_over) return;
    ds->ammo--;
    ds->shoot_flash = 2;

    /* Cast a ray in the player's facing direction */
    int wtype, side;
    int wall_dist = cast_ray(ds, ds->angle, &wtype, &side);

    int player_cos = hal_cos256(ds->angle);
    int player_sin = hal_sin256(ds->angle);

    /* Check each enemy */
    int best_dist = wall_dist;
    int best_idx = -1;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &ds->enemies[i];
        if (!e->alive) continue;

        /* Relative position (Q16 -> Q8) */
        int rel_x = (e->x - ds->px) >> 8;
        int rel_y = (e->y - ds->py) >> 8;

        /* Camera transform */
        int cam_x = (rel_x * player_cos + rel_y * player_sin) >> 8;
        int cam_y = (-rel_x * player_sin + rel_y * player_cos) >> 8;

        if (cam_y < 10) continue;  /* Behind camera */

        /* Check if enemy is near the center line */
        int screen_offset = (cam_x * 100) / cam_y;  /* scaled screen offset */
        if (screen_offset > -20 && screen_offset < 20) {
            /* Within crosshair area */
            if (cam_y < best_dist) {
                best_dist = cam_y;
                best_idx = i;
            }
        }
    }

    if (best_idx >= 0) {
        ds->enemies[best_idx].alive = 0;
        ds->enemies[best_idx].hit_timer = 8;
        ds->score++;
        kprintf("[doom3d] Hit enemy %d! Score: %d\n", best_idx, ds->score);

        /* Check win condition */
        if (ds->score >= MAX_ENEMIES) {
            ds->game_over = 1;
            kprintf("[doom3d] YOU WIN!\n");
        }
    }
}

/* ── Key input ───────────────────────────────────────────────── */
static void doom3d_on_key_down(AppContent *self, uint16_t key, uint16_t mod)
{
    (void)mod;
    DoomState *ds = (DoomState *)self->data;
    if (ds->game_over) return;

    int move_speed = FP_ONE / 5;  /* 0.2 cells per keypress */
    int rot_speed = 8;             /* angle units per keypress */

    int cos_a = hal_cos256(ds->angle);
    int sin_a = hal_sin256(ds->angle);

    switch (key) {
    case HAL_KEY_W:
    case HAL_KEY_UP: {
        int dx = (cos_a * move_speed) >> 8;
        int dy = (sin_a * move_speed) >> 8;
        try_move(ds, dx, dy);
        break;
    }
    case HAL_KEY_S:
    case HAL_KEY_DOWN: {
        int dx = -(cos_a * move_speed) >> 8;
        int dy = -(sin_a * move_speed) >> 8;
        try_move(ds, dx, dy);
        break;
    }
    case HAL_KEY_A:
    case HAL_KEY_LEFT:
        ds->angle = (ds->angle - rot_speed) & 255;
        break;
    case HAL_KEY_D:
    case HAL_KEY_RIGHT:
        ds->angle = (ds->angle + rot_speed) & 255;
        break;
    case HAL_KEY_SPACE:
        do_shoot(ds);
        break;
    }
}

/* ── Cleanup ─────────────────────────────────────────────────── */
static void doom3d_destroy(AppContent *self)
{
    if (self->data) kfree(self->data);
    kfree(self);
}

static int doom3d_on_close(AppContent *self)
{
    (void)self;
    return 1;  /* allow close */
}

/* ── Factory ─────────────────────────────────────────────────── */
AppContent *doom3d_create(void)
{
    AppContent *app = (AppContent *)kmalloc(sizeof(AppContent));
    if (!app) return 0;
    memset(app, 0, sizeof(AppContent));

    DoomState *ds = (DoomState *)kmalloc(sizeof(DoomState));
    if (!ds) { kfree(app); return 0; }
    memset(ds, 0, sizeof(DoomState));

    /* Init player at (2.5, 2.5) */
    ds->px = INT_TO_FP(2) + FP_HALF;
    ds->py = INT_TO_FP(2) + FP_HALF;
    ds->angle = 0;
    ds->health = 100;
    ds->ammo = 50;
    ds->score = 0;
    ds->game_over = 0;

    /* Init enemies */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        ds->enemies[i].x = INT_TO_FP(enemy_start_x[i]) + FP_HALF;
        ds->enemies[i].y = INT_TO_FP(enemy_start_y[i]) + FP_HALF;
        ds->enemies[i].alive = 1;
        ds->enemies[i].hit_timer = 0;
    }

    app->data = ds;
    app->render = doom3d_render;
    app->on_key_down = doom3d_on_key_down;
    app->on_close = doom3d_on_close;
    app->destroy = doom3d_destroy;
    app->on_mouse_down = 0;
    app->on_mouse_up = 0;
    app->on_text_input = 0;
    app->on_scroll = 0;
    app->on_resize = 0;

    kprintf("[doom3d] Game initialized — WASD to move, Space to shoot\n");
    return app;
}
