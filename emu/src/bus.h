#ifndef BUS_H
#define BUS_H

#include "emu.h"

typedef struct {
    int      exception;   /* non-zero ⇒ access fault */
    uint32_t fault_addr;
} bus_result_t;

void     bus_init(uint8_t *ram, size_t ram_size);
uint32_t bus_read32(uint32_t addr, bus_result_t *res);
uint16_t bus_read16(uint32_t addr, bus_result_t *res);
uint8_t  bus_read8 (uint32_t addr, bus_result_t *res);
void     bus_write32(uint32_t addr, uint32_t val, bus_result_t *res);
void     bus_write16(uint32_t addr, uint16_t val, bus_result_t *res);
void     bus_write8 (uint32_t addr, uint8_t  val, bus_result_t *res);

#endif
