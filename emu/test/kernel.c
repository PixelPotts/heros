#include <stdint.h>

/* ── MMIO addresses ─────────────────────────────────────────────── */
#define UART_DATA       (*(volatile uint8_t  *)0x10000000)
#define UART_LSR        (*(volatile uint8_t  *)0x10000005)

#define CLINT_MTIMECMP_LO (*(volatile uint32_t *)0x02004000)
#define CLINT_MTIMECMP_HI (*(volatile uint32_t *)0x02004004)
#define CLINT_MTIME_LO    (*(volatile uint32_t *)0x0200BFF8)
#define CLINT_MTIME_HI    (*(volatile uint32_t *)0x0200BFFC)

#define FB_BASE         ((volatile uint8_t *)0x20000000)
#define FB_CTRL_FLUSH   (*(volatile uint32_t *)0x21000008)
#define FB_WIDTH        2560
#define FB_HEIGHT       1440

/* ── UART ───────────────────────────────────────────────────────── */
static void uart_putchar(char c)
{
    while (!(UART_LSR & (1 << 5)))
        ;   /* wait for TX ready */
    UART_DATA = c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putchar(*s++);
}

static void uart_puthex(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        uart_putchar(hex[(val >> i) & 0xF]);
}

/* Division by 10 using only shifts/adds (no libgcc needed on rv32i) */
static uint32_t div10(uint32_t n)
{
    /* n/10 = (n >> 1) + (n >> 2), refine with correction */
    uint32_t q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q >> 3;
    /* Correct rounding: if q*10 > n, back off */
    uint32_t r = n - ((q << 3) + (q << 1));
    return q + (r > 9);
}

static void uart_putdec(uint32_t val)
{
    char buf[12];
    int i = 0;
    if (val == 0) { uart_putchar('0'); return; }
    while (val > 0) {
        uint32_t q = div10(val);
        uint32_t r = val - ((q << 3) + (q << 1));  /* val - q*10 */
        buf[i++] = '0' + r;
        val = q;
    }
    while (--i >= 0)
        uart_putchar(buf[i]);
}

/* ── Framebuffer ────────────────────────────────────────────────── */
static void fb_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    uint32_t off = (y * FB_WIDTH + x) * 4;
    FB_BASE[off + 0] = r;
    FB_BASE[off + 1] = g;
    FB_BASE[off + 2] = b;
    FB_BASE[off + 3] = 255;   /* alpha */
}

static void fb_fill_rect(int x0, int y0, int w, int h,
                          uint8_t r, uint8_t g, uint8_t b)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            fb_set_pixel(x, y, r, g, b);
}

/* ── Timer ──────────────────────────────────────────────────────── */
#define TIMER_INTERVAL  1000000   /* mtime ticks between interrupts */

static void timer_set_next(void)
{
    uint32_t lo = CLINT_MTIME_LO;
    uint32_t hi = CLINT_MTIME_HI;
    uint64_t mtime = ((uint64_t)hi << 32) | lo;
    uint64_t next  = mtime + TIMER_INTERVAL;
    CLINT_MTIMECMP_LO = (uint32_t)(next);
    CLINT_MTIMECMP_HI = (uint32_t)(next >> 32);
}

/* CSR helpers (inline asm for RV32I) */
static inline uint32_t csr_read_mstatus(void)
{
    uint32_t val;
    __asm__ volatile("csrr %0, mstatus" : "=r"(val));
    return val;
}

static inline void csr_set_bits_mstatus(uint32_t bits)
{
    __asm__ volatile("csrs mstatus, %0" :: "r"(bits));
}

static inline void csr_set_bits_mie(uint32_t bits)
{
    __asm__ volatile("csrs mie, %0" :: "r"(bits));
}

/* ── Trap handler ───────────────────────────────────────────────── */
static volatile uint32_t tick_count = 0;

uint32_t trap_handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    if (mcause == 0x80000007) {
        /* Machine timer interrupt */
        tick_count++;
        uart_puts("[tick ");
        uart_putdec(tick_count);
        uart_puts("]\n");
        timer_set_next();
        return mepc;           /* resume where we were interrupted */
    }

    /* Unknown trap — print info and advance past the instruction */
    uart_puts("TRAP: mcause=");
    uart_puthex(mcause);
    uart_puts(" mepc=");
    uart_puthex(mepc);
    uart_puts(" mtval=");
    uart_puthex(mtval);
    uart_putchar('\n');
    return mepc + 4;
}

/* ── Kernel main ────────────────────────────────────────────────── */
void kernel_main(void)
{
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("  HerOS Kernel v0.1 booting...\n");
    uart_puts("========================================\n");
    uart_puts("\n");

    /* Draw background (dark blue) */
    uart_puts("[kernel] Drawing framebuffer...\n");
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 20, 20, 60);

    /* Draw colored rectangles (scaled for 2560x1440) */
    fb_fill_rect(320, 240, 640, 360, 255, 50, 50);    /* red */
    fb_fill_rect(1120, 240, 640, 360, 50, 255, 50);   /* green */
    fb_fill_rect(720, 720, 640, 360, 50, 100, 255);   /* blue */

    /* Draw a white border */
    for (int x = 160; x < 2400; x++) {
        fb_set_pixel(x, 120, 255, 255, 255);
        fb_set_pixel(x, 1200, 255, 255, 255);
    }
    for (int y = 120; y < 1200; y++) {
        fb_set_pixel(160, y, 255, 255, 255);
        fb_set_pixel(2399, y, 255, 255, 255);
    }

    /* Flush framebuffer */
    FB_CTRL_FLUSH = 1;
    uart_puts("[kernel] Framebuffer drawn and flushed.\n");

    /* Set up timer interrupts */
    uart_puts("[kernel] Setting up timer interrupts...\n");
    timer_set_next();

    /* Enable machine timer interrupt (MIE bit 7) */
    csr_set_bits_mie(1 << 7);

    /* Enable global machine interrupts (MSTATUS.MIE bit 3) */
    csr_set_bits_mstatus(1 << 3);

    uart_puts("[kernel] Timer enabled. Entering main loop.\n");
    uart_puts("\n");

    /* Spin forever — timer interrupts will fire */
    while (1) {
        __asm__ volatile("wfi");
    }
}
