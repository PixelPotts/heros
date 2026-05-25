/*
 * hal_image.c — Image decoding via stb_image (bare-metal config)
 *
 * stb_image.h includes <stdlib.h> and <string.h> which don't exist
 * in our freestanding environment.  We pre-include our own headers
 * and define guards so the #includes inside stb become no-ops.
 */
#include "hal_image.h"
#include "hal_mem.h"
#include "hal_fs.h"
#include "../kernel/string.h"

/* abs() — used by BMP loader (stb_image calls abs() for BMP row order) */
static int abs(int x) { return x < 0 ? -x : x; }

/* ── stb_image configuration ────────────────────────────────── */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_MALLOC(sz)                    hal_malloc(sz)
#define STBI_REALLOC_SIZED(p,oldsz,newsz)  hal_realloc(p, oldsz, newsz)
#define STBI_FREE(p)                       hal_free(p)
#define STBI_ASSERT(x)                     ((void)(x))

/* Suppress warnings for unused static funcs in stb */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include "../vendor/stb_image.h"

#pragma GCC diagnostic pop

/* ── Max limits ──────────────────────────────────────────────── */
#define MAX_FILE_SIZE   (4 * 1024 * 1024)   /* 4 MB */
#define MAX_DIM         4096

/* ── Extension check ─────────────────────────────────────────── */
static int ext_eq(const char *dot, const char *ext)
{
    while (*dot && *ext) {
        char a = *dot, b = *ext;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
        dot++; ext++;
    }
    return *dot == '\0' && *ext == '\0';
}

int hal_image_is_supported(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    static const char *exts[] = {
        ".bmp", ".png", ".jpg", ".jpeg", ".gif",
        ".tga", ".psd", ".ppm", (void *)0
    };
    for (int i = 0; exts[i]; i++) {
        if (ext_eq(dot, exts[i])) return 1;
    }
    return 0;
}

/* ── Load image from filesystem ──────────────────────────────── */
int hal_image_load(const char *path, hal_image_t *img)
{
    if (!path || !img) return -1;
    img->pixels = (void *)0;
    img->width = img->height = 0;

    /* Stat to get size */
    fs_stat_t st;
    if (hal_fs_stat(path, &st) < 0) return -1;
    if (st.size == 0 || st.size > MAX_FILE_SIZE) return -1;

    /* Read entire file into temp buffer */
    uint8_t *buf = (uint8_t *)hal_malloc(st.size);
    if (!buf) return -1;

    int fd = hal_fs_open(path, FS_O_READ);
    if (fd < 0) { hal_free(buf); return -1; }

    int total = 0;
    while (total < (int)st.size) {
        int n = hal_fs_read(fd, buf + total, (int)st.size - total);
        if (n <= 0) break;
        total += n;
    }
    hal_fs_close(fd);

    if (total <= 0) { hal_free(buf); return -1; }

    /* Decode with stb_image */
    int w, h, channels;
    uint8_t *pixels = stbi_load_from_memory(buf, total, &w, &h, &channels, 4);
    hal_free(buf);

    if (!pixels) return -1;
    if (w > MAX_DIM || h > MAX_DIM || w <= 0 || h <= 0) {
        stbi_image_free(pixels);
        return -1;
    }

    img->pixels = pixels;
    img->width  = w;
    img->height = h;
    return 0;
}

/* ── Nearest-neighbor scale ──────────────────────────────────── */
int hal_image_scale(const hal_image_t *src, int nw, int nh, hal_image_t *dst)
{
    if (!src || !src->pixels || !dst) return -1;
    if (nw <= 0 || nh <= 0 || nw > MAX_DIM || nh > MAX_DIM) return -1;

    uint8_t *out = (uint8_t *)hal_malloc((size_t)nw * nh * 4);
    if (!out) return -1;

    int sw = src->width;
    int sh = src->height;

    for (int y = 0; y < nh; y++) {
        int sy = y * sh / nh;
        if (sy >= sh) sy = sh - 1;
        const uint8_t *srow = src->pixels + sy * sw * 4;
        uint8_t *drow = out + y * nw * 4;
        for (int x = 0; x < nw; x++) {
            int sx = x * sw / nw;
            if (sx >= sw) sx = sw - 1;
            const uint8_t *sp = srow + sx * 4;
            uint8_t *dp = drow + x * 4;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }

    dst->pixels = out;
    dst->width  = nw;
    dst->height = nh;
    return 0;
}

/* ── Free ────────────────────────────────────────────────────── */
void hal_image_free(hal_image_t *img)
{
    if (img && img->pixels) {
        hal_free(img->pixels);
        img->pixels = (void *)0;
        img->width = img->height = 0;
    }
}
