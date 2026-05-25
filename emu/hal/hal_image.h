#ifndef HAL_IMAGE_H
#define HAL_IMAGE_H

#include <stdint.h>

typedef struct {
    uint8_t *pixels;  /* RGBA8888 */
    int width, height;
} hal_image_t;

/* Load image from filesystem path. Returns 0 on success, -1 on error. */
int  hal_image_load(const char *path, hal_image_t *img);

/* Nearest-neighbor scale. Caller must free dst with hal_image_free(). */
int  hal_image_scale(const hal_image_t *src, int nw, int nh, hal_image_t *dst);

/* Free decoded image pixels. */
void hal_image_free(hal_image_t *img);

/* Check if filename has a supported image extension. */
int  hal_image_is_supported(const char *filename);

#endif
