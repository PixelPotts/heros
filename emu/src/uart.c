#include "uart.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>

/* NS16550-style register offsets (simplified) */
#define REG_DATA   0   /* 0x10000000 */
#define REG_IER    1
#define REG_ISR    2
#define REG_LCR    3
#define REG_MCR    4
#define REG_LSR    5   /* 0x10000005 — Line Status Register */

#define LSR_RX_READY  (1 << 0)
#define LSR_TX_READY   (1 << 5)

static struct termios orig_termios;
static bool termios_saved = false;

void uart_init(void)
{
    /* Put stdin in non-blocking, raw mode so we can poll chars */
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        termios_saved = true;

        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

bool uart_has_rx(void)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

uint8_t uart_read(uint32_t offset)
{
    switch (offset) {
    case REG_DATA: {
        uint8_t ch = 0;
        if (read(STDIN_FILENO, &ch, 1) == 1)
            return ch;
        return 0;
    }
    case REG_LSR: {
        uint8_t val = LSR_TX_READY;      /* TX always ready */
        if (uart_has_rx())
            val |= LSR_RX_READY;
        return val;
    }
    default:
        return 0;
    }
}

void uart_write(uint32_t offset, uint8_t val)
{
    if (offset == REG_DATA) {
        putchar(val);
        fflush(stdout);
    }
    /* Other register writes ignored for now */
}
