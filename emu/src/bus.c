#include "bus.h"
#include "uart.h"
#include "clint.h"
#include "disk.h"
#include "fb.h"
#include <stdio.h>

static uint8_t *ram;
static size_t   ram_size;

void bus_init(uint8_t *r, size_t sz)
{
    ram      = r;
    ram_size = sz;
}

/* ── helpers ────────────────────────────────────────────────────── */

static inline bool in_range(uint32_t addr, uint32_t base, uint32_t size)
{
    return addr >= base && addr < base + size;
}

/* ── 8-bit read ─────────────────────────────────────────────────── */
uint8_t bus_read8(uint32_t addr, bus_result_t *res)
{
    res->exception  = 0;
    res->fault_addr = 0;

    if (in_range(addr, RAM_BASE, RAM_SIZE))
        return ram[addr - RAM_BASE];

    if (in_range(addr, UART_BASE, UART_SIZE))
        return uart_read(addr - UART_BASE);

    if (in_range(addr, FB_BASE, FB_SIZE))
        return fb_read(addr - FB_BASE);

    /* For 32-bit-only devices accessed byte-wise, return 0 */
    res->exception  = CAUSE_LOAD_FAULT;
    res->fault_addr = addr;
    return 0;
}

/* ── 16-bit read ────────────────────────────────────────────────── */
uint16_t bus_read16(uint32_t addr, bus_result_t *res)
{
    res->exception  = 0;
    res->fault_addr = 0;

    if (in_range(addr, RAM_BASE, RAM_SIZE) && addr + 1 < RAM_BASE + RAM_SIZE) {
        uint32_t off = addr - RAM_BASE;
        return (uint16_t)ram[off] | ((uint16_t)ram[off + 1] << 8);
    }

    /* Fall back to two byte reads */
    uint8_t lo = bus_read8(addr, res);
    if (res->exception) return 0;
    uint8_t hi = bus_read8(addr + 1, res);
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

/* ── 32-bit read ────────────────────────────────────────────────── */
uint32_t bus_read32(uint32_t addr, bus_result_t *res)
{
    res->exception  = 0;
    res->fault_addr = 0;

    if (in_range(addr, RAM_BASE, RAM_SIZE) && addr + 3 < RAM_BASE + RAM_SIZE) {
        uint32_t off = addr - RAM_BASE;
        return (uint32_t)ram[off]
             | ((uint32_t)ram[off + 1] << 8)
             | ((uint32_t)ram[off + 2] << 16)
             | ((uint32_t)ram[off + 3] << 24);
    }

    if (in_range(addr, CLINT_BASE, CLINT_SIZE))
        return clint_read(addr - CLINT_BASE);

    if (in_range(addr, UART_BASE, UART_SIZE))
        return uart_read(addr - UART_BASE);

    if (in_range(addr, DISK_BASE, DISK_SIZE))
        return disk_read(addr - DISK_BASE);

    if (in_range(addr, FB_CTRL_BASE, FB_CTRL_SIZE))
        return fb_ctrl_read(addr - FB_CTRL_BASE);

    if (in_range(addr, FB_BASE, FB_SIZE)) {
        /* 32-bit read from framebuffer — assemble from bytes */
        uint32_t off = addr - FB_BASE;
        return (uint32_t)fb_read(off)
             | ((uint32_t)fb_read(off + 1) << 8)
             | ((uint32_t)fb_read(off + 2) << 16)
             | ((uint32_t)fb_read(off + 3) << 24);
    }

    res->exception  = CAUSE_LOAD_FAULT;
    res->fault_addr = addr;
    return 0;
}

/* ── 8-bit write ────────────────────────────────────────────────── */
void bus_write8(uint32_t addr, uint8_t val, bus_result_t *res)
{
    res->exception  = 0;
    res->fault_addr = 0;

    if (in_range(addr, RAM_BASE, RAM_SIZE)) {
        ram[addr - RAM_BASE] = val;
        return;
    }

    if (in_range(addr, UART_BASE, UART_SIZE)) {
        uart_write(addr - UART_BASE, val);
        return;
    }

    if (in_range(addr, FB_BASE, FB_SIZE)) {
        fb_write(addr - FB_BASE, val);
        return;
    }

    res->exception  = CAUSE_STORE_FAULT;
    res->fault_addr = addr;
}

/* ── 16-bit write ───────────────────────────────────────────────── */
void bus_write16(uint32_t addr, uint16_t val, bus_result_t *res)
{
    res->exception  = 0;
    res->fault_addr = 0;

    if (in_range(addr, RAM_BASE, RAM_SIZE) && addr + 1 < RAM_BASE + RAM_SIZE) {
        uint32_t off = addr - RAM_BASE;
        ram[off]     = (uint8_t)(val);
        ram[off + 1] = (uint8_t)(val >> 8);
        return;
    }

    bus_write8(addr, (uint8_t)val, res);
    if (res->exception) return;
    bus_write8(addr + 1, (uint8_t)(val >> 8), res);
}

/* ── 32-bit write ───────────────────────────────────────────────── */
void bus_write32(uint32_t addr, uint32_t val, bus_result_t *res)
{
    res->exception  = 0;
    res->fault_addr = 0;

    if (in_range(addr, RAM_BASE, RAM_SIZE) && addr + 3 < RAM_BASE + RAM_SIZE) {
        uint32_t off = addr - RAM_BASE;
        ram[off]     = (uint8_t)(val);
        ram[off + 1] = (uint8_t)(val >> 8);
        ram[off + 2] = (uint8_t)(val >> 16);
        ram[off + 3] = (uint8_t)(val >> 24);
        return;
    }

    if (in_range(addr, CLINT_BASE, CLINT_SIZE)) {
        clint_write(addr - CLINT_BASE, val);
        return;
    }

    if (in_range(addr, UART_BASE, UART_SIZE)) {
        uart_write(addr - UART_BASE, (uint8_t)val);
        return;
    }

    if (in_range(addr, DISK_BASE, DISK_SIZE)) {
        disk_write(addr - DISK_BASE, val);
        return;
    }

    if (in_range(addr, FB_CTRL_BASE, FB_CTRL_SIZE)) {
        fb_ctrl_write(addr - FB_CTRL_BASE, val);
        return;
    }

    if (in_range(addr, FB_BASE, FB_SIZE)) {
        uint32_t off = addr - FB_BASE;
        fb_write(off,     (uint8_t)(val));
        fb_write(off + 1, (uint8_t)(val >> 8));
        fb_write(off + 2, (uint8_t)(val >> 16));
        fb_write(off + 3, (uint8_t)(val >> 24));
        return;
    }

    res->exception  = CAUSE_STORE_FAULT;
    res->fault_addr = addr;
}
