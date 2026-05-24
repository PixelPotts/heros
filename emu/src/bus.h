#ifndef BUS_H
#define BUS_H

#include "emu.h"
#include <string.h>

typedef struct {
    int      exception;   /* non-zero ⇒ access fault */
    uint32_t fault_addr;
} bus_result_t;

void     bus_init(uint8_t *ram, size_t ram_size);
uint8_t *bus_get_ram(void);
uint32_t bus_read32(uint32_t addr, bus_result_t *res);
uint16_t bus_read16(uint32_t addr, bus_result_t *res);
uint8_t  bus_read8 (uint32_t addr, bus_result_t *res);
void     bus_write32(uint32_t addr, uint32_t val, bus_result_t *res);
void     bus_write16(uint32_t addr, uint16_t val, bus_result_t *res);
void     bus_write8 (uint32_t addr, uint8_t  val, bus_result_t *res);

/* Fast instruction fetch — inline for hot path.
 * Returns 0 and sets res->exception on miss. */
static inline uint32_t bus_fetch32(uint32_t addr, uint8_t *ram_ptr, bus_result_t *res)
{
    if (__builtin_expect(addr >= RAM_BASE && addr + 3 < RAM_BASE + RAM_SIZE, 1)) {
        uint32_t val;
        memcpy(&val, ram_ptr + (addr - RAM_BASE), 4);
        res->exception = 0;
        return val;
    }
    return bus_read32(addr, res);
}

#endif
