/*
 * mktest_images — Generate 10 test images for HerOS
 *
 * Uses stb_image_write.h to create BMP, PNG, TGA, and JPEG files.
 * All filenames ≤ 8 chars (FAT16 8.3).
 *
 * Usage: mktest_images <output_dir>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../vendor/stb_image_write.h"

/* ── Helpers ─────────────────────────────────────────────────── */

static void set_pixel(uint8_t *buf, int w, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    int off = (y * w + x) * 3;
    buf[off]     = r;
    buf[off + 1] = g;
    buf[off + 2] = b;
}

static void fill_rect(uint8_t *buf, int bw, int x0, int y0, int w, int h,
                       uint8_t r, uint8_t g, uint8_t b)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            set_pixel(buf, bw, x, y, r, g, b);
}

/* Draw a single character (5x7 bitmap font, crude) */
static const uint8_t font5x7[128][7] = {
    ['H'] = {0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
    ['e'] = {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E},
    ['r'] = {0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10},
    ['O'] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['S'] = {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},
};

static void draw_char(uint8_t *buf, int bw, int cx, int cy,
                       char ch, uint8_t r, uint8_t g, uint8_t b, int scale)
{
    if (ch < 0 || ch > 127) return;
    for (int row = 0; row < 7; row++) {
        uint8_t bits = font5x7[(int)ch][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        set_pixel(buf, bw, cx + col * scale + sx,
                                  cy + row * scale + sy, r, g, b);
            }
        }
    }
}

static void draw_string(uint8_t *buf, int bw, int x, int y,
                          const char *s, uint8_t r, uint8_t g, uint8_t b, int scale)
{
    while (*s) {
        draw_char(buf, bw, x, y, *s, r, g, b, scale);
        x += 6 * scale;
        s++;
    }
}

static void make_path(const char *dir, const char *name, char *out)
{
    strcpy(out, dir);
    int len = strlen(out);
    if (len > 0 && out[len - 1] != '/') { out[len] = '/'; out[len + 1] = '\0'; }
    strcat(out, name);
}

/* ── Image generators ────────────────────────────────────────── */

/* 1. red.bmp — 64x64 solid red */
static void gen_red(const char *dir)
{
    int w = 64, h = 64;
    uint8_t *buf = calloc(w * h * 3, 1);
    fill_rect(buf, w, 0, 0, w, h, 255, 0, 0);
    char path[256]; make_path(dir, "red.bmp", path);
    stbi_write_bmp(path, w, h, 3, buf);
    free(buf);
    printf("  %s\n", path);
}

/* 2. green.png — 64x64 solid green */
static void gen_green(const char *dir)
{
    int w = 64, h = 64;
    uint8_t *buf = calloc(w * h * 3, 1);
    fill_rect(buf, w, 0, 0, w, h, 0, 200, 0);
    char path[256]; make_path(dir, "green.png", path);
    stbi_write_png(path, w, h, 3, buf, w * 3);
    free(buf);
    printf("  %s\n", path);
}

/* 3. blue.jpg — 64x64 solid blue */
static void gen_blue(const char *dir)
{
    int w = 64, h = 64;
    uint8_t *buf = calloc(w * h * 3, 1);
    fill_rect(buf, w, 0, 0, w, h, 0, 0, 255);
    char path[256]; make_path(dir, "blue.jpg", path);
    stbi_write_jpg(path, w, h, 3, buf, 90);
    free(buf);
    printf("  %s\n", path);
}

/* 4. gradient.bmp — 128x128 red→blue horizontal gradient */
static void gen_gradient(const char *dir)
{
    int w = 128, h = 128;
    uint8_t *buf = calloc(w * h * 3, 1);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t r = 255 - (x * 255 / w);
            uint8_t b = x * 255 / w;
            set_pixel(buf, w, x, y, r, 0, b);
        }
    char path[256]; make_path(dir, "gradient.bmp", path);
    stbi_write_bmp(path, w, h, 3, buf);
    free(buf);
    printf("  %s\n", path);
}

/* 5. checker.png — 128x128 B/W checkerboard */
static void gen_checker(const char *dir)
{
    int w = 128, h = 128, cs = 16;
    uint8_t *buf = calloc(w * h * 3, 1);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int dark = ((x / cs) + (y / cs)) & 1;
            uint8_t v = dark ? 40 : 220;
            set_pixel(buf, w, x, y, v, v, v);
        }
    char path[256]; make_path(dir, "checker.png", path);
    stbi_write_png(path, w, h, 3, buf, w * 3);
    free(buf);
    printf("  %s\n", path);
}

/* 6. stripes.tga — 100x100 rainbow horizontal stripes */
static void gen_stripes(const char *dir)
{
    int w = 100, h = 100;
    uint8_t *buf = calloc(w * h * 3, 1);
    /* 7 rainbow stripes */
    uint8_t colors[][3] = {
        {255, 0, 0}, {255, 127, 0}, {255, 255, 0},
        {0, 200, 0}, {0, 0, 255}, {75, 0, 130}, {148, 0, 211}
    };
    for (int y = 0; y < h; y++) {
        int ci = y * 7 / h;
        if (ci > 6) ci = 6;
        for (int x = 0; x < w; x++)
            set_pixel(buf, w, x, y, colors[ci][0], colors[ci][1], colors[ci][2]);
    }
    char path[256]; make_path(dir, "stripes.tga", path);
    stbi_write_tga(path, w, h, 3, buf);
    free(buf);
    printf("  %s\n", path);
}

/* 7. rgbtest.bmp — 192x128 R/G/B vertical bars */
static void gen_rgbtest(const char *dir)
{
    int w = 192, h = 128;
    uint8_t *buf = calloc(w * h * 3, 1);
    fill_rect(buf, w, 0,   0, 64, h, 255, 0, 0);
    fill_rect(buf, w, 64,  0, 64, h, 0, 255, 0);
    fill_rect(buf, w, 128, 0, 64, h, 0, 0, 255);
    char path[256]; make_path(dir, "rgbtest.bmp", path);
    stbi_write_bmp(path, w, h, 3, buf);
    free(buf);
    printf("  %s\n", path);
}

/* 8. heros.png — 160x120 pixel-art "HerOS" text */
static void gen_heros(const char *dir)
{
    int w = 160, h = 120;
    uint8_t *buf = calloc(w * h * 3, 1);
    /* Dark blue background */
    fill_rect(buf, w, 0, 0, w, h, 20, 30, 60);
    /* Draw "HerOS" centered */
    draw_string(buf, w, 20, 45, "HerOS", 255, 220, 80, 4);
    char path[256]; make_path(dir, "heros.png", path);
    stbi_write_png(path, w, h, 3, buf, w * 3);
    free(buf);
    printf("  %s\n", path);
}

/* 9. mandel.bmp — 200x150 Mandelbrot (fixed-point) */
static void gen_mandel(const char *dir)
{
    int w = 200, h = 150;
    uint8_t *buf = calloc(w * h * 3, 1);
    /* Fixed-point: 16.16 format */
    for (int py = 0; py < h; py++)
        for (int px = 0; px < w; px++) {
            /* Map pixel to complex plane: [-2.5, 1.0] x [-1.2, 1.2] */
            int32_t cr = -163840 + (int32_t)px * 229376 / w;  /* -2.5 to 1.0 */
            int32_t ci = -78643 + (int32_t)py * 157286 / h;   /* -1.2 to 1.2 */
            int32_t zr = 0, zi = 0;
            int iter = 0, max_iter = 32;
            while (iter < max_iter) {
                int32_t zr2 = (int32_t)(((int64_t)zr * zr) >> 16);
                int32_t zi2 = (int32_t)(((int64_t)zi * zi) >> 16);
                if (zr2 + zi2 > 4 * 65536) break;
                int32_t nzr = zr2 - zi2 + cr;
                zi = (int32_t)(((int64_t)zr * zi) >> 15) + ci;
                zr = nzr;
                iter++;
            }
            uint8_t r = 0, g = 0, b = 0;
            if (iter < max_iter) {
                r = (iter * 8) & 0xFF;
                g = (iter * 12) & 0xFF;
                b = (iter * 16 + 80) & 0xFF;
            }
            set_pixel(buf, w, px, py, r, g, b);
        }
    char path[256]; make_path(dir, "mandel.bmp", path);
    stbi_write_bmp(path, w, h, 3, buf);
    free(buf);
    printf("  %s\n", path);
}

/* 10. landscap.jpg — 256x192 sky gradient + hills + sun */
static void gen_landscape(const char *dir)
{
    int w = 256, h = 192;
    uint8_t *buf = calloc(w * h * 3, 1);

    /* Sky gradient */
    for (int y = 0; y < h; y++) {
        uint8_t r = 30 + y * 100 / h;
        uint8_t g = 100 + y * 80 / h;
        uint8_t b = 220 - y * 40 / h;
        for (int x = 0; x < w; x++)
            set_pixel(buf, w, x, y, r, g, b);
    }

    /* Sun */
    int sx = 200, sy = 40, sr = 20;
    for (int y = sy - sr; y <= sy + sr; y++)
        for (int x = sx - sr; x <= sx + sr; x++) {
            if (x < 0 || x >= w || y < 0 || y >= h) continue;
            int dx = x - sx, dy = y - sy;
            if (dx * dx + dy * dy <= sr * sr)
                set_pixel(buf, w, x, y, 255, 230, 80);
        }

    /* Rolling hills */
    for (int x = 0; x < w; x++) {
        /* Simple sine-ish hills using integer math */
        int hill1 = 140 + 20 * (x % 80 < 40 ? (x % 40 - 20) : (20 - x % 40)) / 20;
        int hill2 = 150 + 15 * ((x + 30) % 60 < 30 ? ((x + 30) % 30 - 15) : (15 - (x + 30) % 30)) / 15;
        int top = hill1 < hill2 ? hill1 : hill2;
        for (int y = top; y < h; y++) {
            uint8_t g = 80 + (h - y) * 2;
            if (g > 180) g = 180;
            set_pixel(buf, w, x, y, 40, g, 30);
        }
    }

    char path[256]; make_path(dir, "landscap.jpg", path);
    stbi_write_jpg(path, w, h, 3, buf, 85);
    free(buf);
    printf("  %s\n", path);
}

/* ── Main ────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_dir>\n", argv[0]);
        return 1;
    }

    const char *dir = argv[1];
    printf("Generating test images in %s/\n", dir);

    gen_red(dir);
    gen_green(dir);
    gen_blue(dir);
    gen_gradient(dir);
    gen_checker(dir);
    gen_stripes(dir);
    gen_rgbtest(dir);
    gen_heros(dir);
    gen_mandel(dir);
    gen_landscape(dir);

    printf("Done: 10 test images generated\n");
    return 0;
}
