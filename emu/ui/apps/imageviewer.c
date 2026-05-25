/*
 * imageviewer.c — Image viewer application for HerOS
 *
 * Supports BMP, PNG, JPEG, GIF, TGA, PSD, PPM via stb_image.
 * Features: fit-to-window, 100% zoom, pan with arrow keys.
 */
#include "../types.h"
#include "../draw.h"
#include "../theme.h"
#include "../icons.h"
#include "../window.h"
#include "../../kernel/string.h"
#include "../../kernel/mm.h"
#include "../../hal/hal_image.h"

#define IV_STATUS_H  20
#define IV_PAN_STEP  20

/* Zoom modes */
#define ZOOM_FIT   0
#define ZOOM_100   1

typedef struct {
    char          path[256];
    hal_image_t   img;          /* decoded full image */
    hal_image_t   scaled;       /* cached scaled copy (for fit mode) */
    int           scaled_cw;    /* content w/h when scaled was generated */
    int           scaled_ch;
    int           pan_x, pan_y; /* pan offset in 100% mode */
    int           zoom;         /* ZOOM_FIT or ZOOM_100 */
    int           loaded;       /* 1 if image loaded OK */
    char          err[64];      /* error message if load failed */
} ImgViewData;

/* ── Helpers ─────────────────────────────────────────────────── */

static const char *basename(const char *path)
{
    const char *sl = strrchr(path, '/');
    return sl ? sl + 1 : path;
}

static const char *get_format(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return "???";
    if (strcmp(dot, ".bmp") == 0)  return "BMP";
    if (strcmp(dot, ".png") == 0)  return "PNG";
    if (strcmp(dot, ".jpg") == 0)  return "JPEG";
    if (strcmp(dot, ".jpeg") == 0) return "JPEG";
    if (strcmp(dot, ".gif") == 0)  return "GIF";
    if (strcmp(dot, ".tga") == 0)  return "TGA";
    if (strcmp(dot, ".psd") == 0)  return "PSD";
    if (strcmp(dot, ".ppm") == 0)  return "PPM";
    return "???";
}

static void invalidate_scaled(ImgViewData *iv)
{
    hal_image_free(&iv->scaled);
    iv->scaled_cw = 0;
    iv->scaled_ch = 0;
}

/* ── Render ──────────────────────────────────────────────────── */

static void iv_render(AppContent *self, Rect cr)
{
    ImgViewData *iv = (ImgViewData *)self->data;
    const ThemeColors *tc = theme_current();

    /* Background */
    draw_filled_rect(cr, tc->bg_secondary);

    int view_w = cr.w;
    int view_h = cr.h - IV_STATUS_H;

    if (!iv->loaded) {
        /* Error message */
        draw_text_centered(cr.x + cr.w / 2, cr.y + cr.h / 2 - 6,
                          iv->err[0] ? iv->err : "No image",
                          tc->text_muted, FONT_SIZE_SMALL);
        return;
    }

    int iw = iv->img.width;
    int ih = iv->img.height;

    if (iv->zoom == ZOOM_FIT) {
        /* Scale to fit within view area */
        int dw, dh;
        if (iw <= view_w && ih <= view_h) {
            /* Image smaller than view — show at 100% */
            dw = iw;
            dh = ih;
        } else {
            /* Scale down maintaining aspect ratio */
            int sw = view_w * 1000 / iw;
            int sh = view_h * 1000 / ih;
            int s = sw < sh ? sw : sh;
            dw = iw * s / 1000;
            dh = ih * s / 1000;
            if (dw < 1) dw = 1;
            if (dh < 1) dh = 1;
        }

        /* Generate scaled cache if needed */
        if (iv->scaled_cw != view_w || iv->scaled_ch != view_h) {
            invalidate_scaled(iv);
            if (dw != iw || dh != ih) {
                hal_image_scale(&iv->img, dw, dh, &iv->scaled);
            }
            iv->scaled_cw = view_w;
            iv->scaled_ch = view_h;
        }

        /* Center and blit */
        const hal_image_t *src = iv->scaled.pixels ? &iv->scaled : &iv->img;
        int dx = cr.x + (view_w - src->width) / 2;
        int dy = cr.y + (view_h - src->height) / 2;
        hal_fb_blit(dx, dy, src->width, src->height,
                    src->pixels, src->width * 4);
    } else {
        /* 100% zoom with panning */
        /* Clamp pan */
        int max_px = iw > view_w ? iw - view_w : 0;
        int max_py = ih > view_h ? ih - view_h : 0;
        if (iv->pan_x < 0) iv->pan_x = 0;
        if (iv->pan_y < 0) iv->pan_y = 0;
        if (iv->pan_x > max_px) iv->pan_x = max_px;
        if (iv->pan_y > max_py) iv->pan_y = max_py;

        /* Calculate visible region */
        int sx = iv->pan_x;
        int sy = iv->pan_y;
        int bw = iw - sx;
        int bh = ih - sy;
        if (bw > view_w) bw = view_w;
        if (bh > view_h) bh = view_h;

        /* Center if image smaller than view */
        int dx = cr.x;
        int dy = cr.y;
        if (iw < view_w) dx += (view_w - iw) / 2;
        if (ih < view_h) dy += (view_h - ih) / 2;

        /* Blit visible portion */
        const uint8_t *src = iv->img.pixels + (sy * iw + sx) * 4;
        hal_fb_blit(dx, dy, bw, bh, src, iw * 4);
    }

    /* ── Status bar ──────────────────────────────────────────── */
    int sy = cr.y + cr.h - IV_STATUS_H;
    draw_filled_rect(RECT(cr.x, sy, cr.w, IV_STATUS_H), tc->panel_bg);
    draw_line(cr.x, sy, cr.x + cr.w, sy, tc->panel_border);

    /* File name */
    draw_text(cr.x + 6, sy + 4, basename(iv->path),
              tc->text_primary, FONT_SIZE_SMALL);

    /* Dimensions + format */
    char info[64];
    char wbuf[12], hbuf[12];
    itoa(iw, wbuf, 10);
    itoa(ih, hbuf, 10);
    strcpy(info, wbuf);
    strcat(info, "x");
    strcat(info, hbuf);
    strcat(info, " ");
    strcat(info, get_format(iv->path));
    strcat(info, iv->zoom == ZOOM_FIT ? " [Fit]" : " [100%]");
    draw_text_right(cr.x + cr.w - 6, sy + 4, info,
                    tc->text_muted, FONT_SIZE_SMALL);
}

/* ── Key handler ─────────────────────────────────────────────── */

static void iv_on_key_down(AppContent *self, uint16_t key, uint16_t mod)
{
    (void)mod;
    ImgViewData *iv = (ImgViewData *)self->data;
    if (!iv->loaded) return;

    switch (key) {
    case 'f': case 'F':
        iv->zoom = ZOOM_FIT;
        iv->pan_x = iv->pan_y = 0;
        invalidate_scaled(iv);
        break;
    case '1':
        iv->zoom = ZOOM_100;
        iv->pan_x = iv->pan_y = 0;
        break;
    case 0x4000004F: /* RIGHT */
        if (iv->zoom == ZOOM_100) iv->pan_x += IV_PAN_STEP;
        break;
    case 0x40000050: /* LEFT */
        if (iv->zoom == ZOOM_100) iv->pan_x -= IV_PAN_STEP;
        break;
    case 0x40000051: /* DOWN */
        if (iv->zoom == ZOOM_100) iv->pan_y += IV_PAN_STEP;
        break;
    case 0x40000052: /* UP */
        if (iv->zoom == ZOOM_100) iv->pan_y -= IV_PAN_STEP;
        break;
    }
}

/* ── Resize ──────────────────────────────────────────────────── */

static void iv_on_resize(AppContent *self, int w, int h)
{
    (void)w; (void)h;
    ImgViewData *iv = (ImgViewData *)self->data;
    invalidate_scaled(iv);
}

/* ── Cleanup ─────────────────────────────────────────────────── */

static void iv_destroy(AppContent *self)
{
    ImgViewData *iv = (ImgViewData *)self->data;
    hal_image_free(&iv->img);
    hal_image_free(&iv->scaled);
    hal_free(iv);
    hal_free(self);
}

/* ── Factory ─────────────────────────────────────────────────── */

AppContent *imageviewer_create_with_file(const char *path)
{
    AppContent *ac = (AppContent *)hal_malloc(sizeof(AppContent));
    if (!ac) return (void *)0;
    memset(ac, 0, sizeof(AppContent));

    ImgViewData *iv = (ImgViewData *)hal_malloc(sizeof(ImgViewData));
    if (!iv) { hal_free(ac); return (void *)0; }
    memset(iv, 0, sizeof(ImgViewData));

    strncpy(iv->path, path, 255);
    iv->path[255] = '\0';
    iv->zoom = ZOOM_FIT;

    /* Load image */
    if (hal_image_load(path, &iv->img) == 0) {
        iv->loaded = 1;
    } else {
        strcpy(iv->err, "Failed to load image");
    }

    ac->render       = iv_render;
    ac->on_key_down  = iv_on_key_down;
    ac->on_resize    = iv_on_resize;
    ac->destroy      = iv_destroy;
    ac->data         = iv;
    return ac;
}
