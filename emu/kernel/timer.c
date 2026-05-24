#include "timer.h"
#include "kprintf.h"

/* CLINT MMIO addresses */
#define CLINT_MTIMECMP_LO   (*(volatile uint32_t *)0x02004000)
#define CLINT_MTIMECMP_HI   (*(volatile uint32_t *)0x02004004)
#define CLINT_MTIME_LO      (*(volatile uint32_t *)0x0200BFF8)
#define CLINT_MTIME_HI      (*(volatile uint32_t *)0x0200BFFC)

/* How many CLINT ticks per timer interrupt */
#define TIMER_INTERVAL      (TIMER_FREQ_HZ / (1000 / TIMER_QUANTUM_MS))

void timer_init(void)
{
    kprintf("[timer] CLINT timer, %u Hz, quantum %u ms\n",
            TIMER_FREQ_HZ, TIMER_QUANTUM_MS);
    timer_set_next();
}

void timer_set_next(void)
{
    uint64_t mtime = timer_get_ticks();
    uint64_t next = mtime + TIMER_INTERVAL;
    /* Write hi first to avoid spurious interrupt */
    CLINT_MTIMECMP_HI = (uint32_t)(next >> 32);
    CLINT_MTIMECMP_LO = (uint32_t)(next);
}

uint64_t timer_get_ticks(void)
{
    uint32_t hi, lo;
    do {
        hi = CLINT_MTIME_HI;
        lo = CLINT_MTIME_LO;
    } while (hi != CLINT_MTIME_HI);
    return ((uint64_t)hi << 32) | lo;
}

uint32_t timer_uptime_ms(void)
{
    uint64_t ticks = timer_get_ticks();
    /* ticks / (TIMER_FREQ_HZ / 1000) = ticks * 1000 / TIMER_FREQ_HZ */
    /* To avoid 64-bit overflow, divide first */
    return (uint32_t)(ticks / (TIMER_FREQ_HZ / 1000));
}
