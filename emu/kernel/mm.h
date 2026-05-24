#ifndef KERNEL_MM_H
#define KERNEL_MM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE     4096
#define PAGE_SHIFT    12

/* Initialize memory manager with heap region */
void  mm_init(uint32_t heap_start, uint32_t heap_end);

/* Page allocator */
void *page_alloc(size_t count);    /* allocate count contiguous pages */
void  page_free(void *addr, size_t count);

/* Slab allocator (kmalloc/kfree) */
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *krealloc(void *ptr, size_t old_size, size_t new_size);

/* Stats */
size_t mm_free_pages(void);
size_t mm_total_pages(void);
size_t mm_used_bytes(void);

#endif
