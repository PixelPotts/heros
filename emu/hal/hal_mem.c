#include "hal_mem.h"
#include "../kernel/mm.h"

void *hal_malloc(size_t size)
{
    return kmalloc(size);
}

void hal_free(void *ptr)
{
    kfree(ptr);
}

void *hal_realloc(void *ptr, size_t old_size, size_t new_size)
{
    return krealloc(ptr, old_size, new_size);
}
