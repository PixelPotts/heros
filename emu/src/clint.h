#ifndef CLINT_H
#define CLINT_H

#include "emu.h"

void     clint_init(void);
uint32_t clint_read(uint32_t offset);
void     clint_write(uint32_t offset, uint32_t val);
void     clint_tick(uint32_t ticks);
bool     clint_timer_pending(void);
bool     clint_software_pending(void);

#endif
