#include "uart.h"
#include "kprintf.h"
#include "mm.h"
#include "timer.h"
#include "sched.h"
#include "disk.h"
#include "input.h"
#include "fb.h"
#include "fs.h"
#include "string.h"

/* Linker symbols */
extern char __bss_start[], __bss_end[];
extern char _heap_start[], _heap_end[];

/* Forward declarations for desktop entry */
extern void desktop_main(void);

/* ── Test task: prints a message periodically ────────────────────── */
static void test_task_a(void)
{
    for (int i = 0; i < 5; i++) {
        kprintf("[Task A] tick %d\n", i);
        sched_sleep_ms(500);
    }
    kprintf("[Task A] done\n");
}

static void test_task_b(void)
{
    for (int i = 0; i < 5; i++) {
        kprintf("[Task B] tick %d\n", i);
        sched_sleep_ms(700);
    }
    kprintf("[Task B] done\n");
}

/* ── Kernel main ─────────────────────────────────────────────────── */
void kmain(void)
{
    uart_init();

    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  HerOS Kernel v1.0\n");
    kprintf("  RISC-V RV32IM bare-metal OS\n");
    kprintf("========================================\n");
    kprintf("\n");

    /* Init memory manager */
    mm_init((uint32_t)(uintptr_t)_heap_start,
            (uint32_t)(uintptr_t)_heap_end);

    /* Init drivers */
    timer_init();
    disk_driver_init();
    input_driver_init();
    fb_driver_init();

    /* Init filesystem */
    fs_init();

    /* Init scheduler */
    sched_init();

    /* Create test tasks */
    sched_create_task("test_a", test_task_a);
    sched_create_task("test_b", test_task_b);

    /* Create desktop task */
    sched_create_task("desktop", desktop_main);

    /* Start scheduler (enables timer interrupts) */
    sched_start();

    kprintf("[kernel] Boot complete. Entering idle loop.\n\n");

    /* Idle loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
