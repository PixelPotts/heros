#ifndef HAL_MEM_H
#define HAL_MEM_H

#include <stddef.h>

void *hal_malloc(size_t size);
void  hal_free(void *ptr);
void *hal_realloc(void *ptr, size_t old_size, size_t new_size);

#endif
