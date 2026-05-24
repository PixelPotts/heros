#ifndef UART_H
#define UART_H

#include "emu.h"

void     uart_init(void);
uint8_t  uart_read(uint32_t offset);
void     uart_write(uint32_t offset, uint8_t val);
bool     uart_has_rx(void);

#endif
