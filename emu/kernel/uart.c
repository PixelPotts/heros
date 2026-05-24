#include "uart.h"

/* NS16550 MMIO registers */
#define UART_BASE       0x10000000
#define REG_DATA        (*(volatile uint8_t *)(UART_BASE + 0))
#define REG_IER         (*(volatile uint8_t *)(UART_BASE + 1))
#define REG_FCR         (*(volatile uint8_t *)(UART_BASE + 2))
#define REG_LCR         (*(volatile uint8_t *)(UART_BASE + 3))
#define REG_MCR         (*(volatile uint8_t *)(UART_BASE + 4))
#define REG_LSR         (*(volatile uint8_t *)(UART_BASE + 5))

#define LSR_RX_READY    (1 << 0)
#define LSR_TX_READY    (1 << 5)

void uart_init(void)
{
    /* Disable interrupts */
    REG_IER = 0x00;
    /* Enable FIFO, clear them */
    REG_FCR = 0x07;
    /* 8 bits, no parity, 1 stop bit */
    REG_LCR = 0x03;
    /* Enable DTR/RTS */
    REG_MCR = 0x03;
}

void uart_putchar(char c)
{
    while (!(REG_LSR & LSR_TX_READY))
        ;
    REG_DATA = (uint8_t)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putchar('\r');
        uart_putchar(*s++);
    }
}

int uart_getchar(void)
{
    if (REG_LSR & LSR_RX_READY)
        return (int)REG_DATA;
    return -1;
}
