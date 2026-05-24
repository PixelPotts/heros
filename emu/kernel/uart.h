#ifndef KERNEL_UART_H
#define KERNEL_UART_H

#include <stdint.h>

void    uart_init(void);
void    uart_putchar(char c);
void    uart_puts(const char *s);
int     uart_getchar(void);   /* non-blocking, returns -1 if no data */

#endif
