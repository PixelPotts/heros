#include "mm.h"
#include "string.h"
#include "kprintf.h"

/* ═══════════════════════════════════════════════════════════════════
   Page allocator — bitmap-based
   ═══════════════════════════════════════════════════════════════════ */

#define MAX_PAGES     32768    /* 128 MB / 4KB = 32K pages */

static uint32_t page_bitmap[MAX_PAGES / 32];
static uint32_t total_pages;
static uint32_t used_pages;
static uint32_t heap_base;

static inline void bitmap_set(uint32_t page)
{
    page_bitmap[page / 32] |= (1U << (page % 32));
}

static inline void bitmap_clear(uint32_t page)
{
    page_bitmap[page / 32] &= ~(1U << (page % 32));
}

static inline int bitmap_test(uint32_t page)
{
    return (page_bitmap[page / 32] >> (page % 32)) & 1;
}

void mm_init(uint32_t start, uint32_t end)
{
    heap_base = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  /* align up */
    uint32_t heap_end = end & ~(PAGE_SIZE - 1);               /* align down */

    total_pages = (heap_end - heap_base) / PAGE_SIZE;
    if (total_pages > MAX_PAGES)
        total_pages = MAX_PAGES;

    /* Mark all pages as free */
    memset(page_bitmap, 0, sizeof(page_bitmap));
    used_pages = 0;

    kprintf("[mm] Heap: 0x%08x - 0x%08x (%u pages, %u MB)\n",
            heap_base, heap_end, total_pages,
            (total_pages * PAGE_SIZE) / (1024 * 1024));
}

void *page_alloc(size_t count)
{
    if (count == 0) return (void *)0;

    /* Find contiguous free pages */
    uint32_t run = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < total_pages; i++) {
        if (bitmap_test(i)) {
            run = 0;
        } else {
            if (run == 0) start = i;
            run++;
            if (run == count) {
                /* Mark pages as used */
                for (uint32_t j = start; j < start + count; j++)
                    bitmap_set(j);
                used_pages += count;
                void *addr = (void *)(heap_base + start * PAGE_SIZE);
                memset(addr, 0, count * PAGE_SIZE);
                return addr;
            }
        }
    }

    kprintf("[mm] page_alloc: out of memory (requested %u pages)\n", (unsigned)count);
    return (void *)0;
}

void page_free(void *addr, size_t count)
{
    uint32_t page = ((uint32_t)(uintptr_t)addr - heap_base) / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        if (bitmap_test(page + i)) {
            bitmap_clear(page + i);
            used_pages--;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
   Slab allocator — simple size-class free lists
   ═══════════════════════════════════════════════════════════════════ */

/* Size classes: 16, 32, 64, 128, 256, 512, 1024, 2048 */
#define SLAB_CLASSES     8
#define SLAB_MIN_SHIFT   4      /* 16 bytes */
#define SLAB_MAX_SIZE    2048

typedef struct free_node {
    struct free_node *next;
} free_node_t;

typedef struct {
    free_node_t *free_list;
    uint32_t     obj_size;
    uint32_t     alloc_count;
} slab_class_t;

static slab_class_t slabs[SLAB_CLASSES];

static int size_to_class(size_t size)
{
    /* Round up to nearest power-of-2 size class */
    int cls = 0;
    size_t s = 1 << SLAB_MIN_SHIFT;  /* 16 */
    while (s < size && cls < SLAB_CLASSES - 1) {
        s <<= 1;
        cls++;
    }
    return cls;
}

static void slab_refill(int cls)
{
    uint32_t obj_size = slabs[cls].obj_size;
    uint32_t objs_per_page = PAGE_SIZE / obj_size;

    void *page = page_alloc(1);
    if (!page) return;

    /* Carve page into objects and link them */
    uint8_t *p = (uint8_t *)page;
    for (uint32_t i = 0; i < objs_per_page; i++) {
        free_node_t *node = (free_node_t *)(p + i * obj_size);
        node->next = slabs[cls].free_list;
        slabs[cls].free_list = node;
    }
}

void *kmalloc(size_t size)
{
    if (size == 0) return (void *)0;

    /* Add header for tracking size class */
    size += sizeof(uint32_t);

    if (size > SLAB_MAX_SIZE) {
        /* Large allocation — use page allocator directly */
        size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        uint32_t *p = (uint32_t *)page_alloc(pages);
        if (!p) return (void *)0;
        p[0] = (uint32_t)pages;  /* store page count in header */
        return (void *)(p + 1);
    }

    int cls = size_to_class(size);
    if (!slabs[cls].free_list)
        slab_refill(cls);
    if (!slabs[cls].free_list)
        return (void *)0;

    free_node_t *node = slabs[cls].free_list;
    slabs[cls].free_list = node->next;
    slabs[cls].alloc_count++;

    /* Store class index in header */
    uint32_t *p = (uint32_t *)node;
    p[0] = (uint32_t)cls | 0x5A000000;  /* magic + class */
    return (void *)(p + 1);
}

void kfree(void *ptr)
{
    if (!ptr) return;

    uint32_t *p = (uint32_t *)ptr - 1;
    uint32_t header = p[0];

    if ((header & 0xFF000000) == 0x5A000000) {
        /* Slab allocation */
        int cls = header & 0xFF;
        if (cls >= SLAB_CLASSES) return;
        free_node_t *node = (free_node_t *)p;
        node->next = slabs[cls].free_list;
        slabs[cls].free_list = node;
        slabs[cls].alloc_count--;
    } else {
        /* Large page allocation */
        uint32_t pages = header;
        page_free(p, pages);
    }
}

void *krealloc(void *ptr, size_t old_size, size_t new_size)
{
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return (void *)0; }

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return (void *)0;

    size_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);
    return new_ptr;
}

size_t mm_free_pages(void)
{
    return total_pages - used_pages;
}

size_t mm_total_pages(void)
{
    return total_pages;
}

size_t mm_used_bytes(void)
{
    size_t total = used_pages * PAGE_SIZE;
    for (int i = 0; i < SLAB_CLASSES; i++)
        total += slabs[i].alloc_count * slabs[i].obj_size;
    return total;
}

/* Initialize slab size classes (called from mm_init context) */
static void __attribute__((constructor)) slab_init_classes(void)
{
    for (int i = 0; i < SLAB_CLASSES; i++) {
        slabs[i].obj_size = 1 << (SLAB_MIN_SHIFT + i);
        slabs[i].free_list = (void *)0;
        slabs[i].alloc_count = 0;
    }
}
