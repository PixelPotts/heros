#include "kprintf.h"
#include "uart.h"
#include "string.h"

static void print_char(char c)
{
    if (c == '\n') uart_putchar('\r');
    uart_putchar(c);
}

static void print_string(const char *s)
{
    if (!s) s = "(null)";
    while (*s) print_char(*s++);
}

static void print_unsigned(unsigned val, int base, int uppercase)
{
    char buf[34];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    while (val > 0) {
        unsigned d = val % (unsigned)base;
        if (d < 10)
            buf[i++] = '0' + d;
        else
            buf[i++] = (uppercase ? 'A' : 'a') + d - 10;
        val /= (unsigned)base;
    }
    while (--i >= 0) uart_putchar(buf[i]);
}

static void print_int(int val)
{
    if (val < 0) {
        uart_putchar('-');
        /* Handle INT_MIN carefully */
        print_unsigned((unsigned)(-(val + 1)) + 1, 10, 0);
    } else {
        print_unsigned((unsigned)val, 10, 0);
    }
}

static void print_hex(unsigned val)
{
    print_unsigned(val, 16, 0);
}

static void print_pointer(unsigned val)
{
    uart_putchar('0');
    uart_putchar('x');
    /* Print with leading zeros for 8 hex digits */
    for (int i = 28; i >= 0; i -= 4) {
        unsigned d = (val >> i) & 0xF;
        uart_putchar(d < 10 ? '0' + d : 'a' + d - 10);
    }
}

void kvprintf(const char *fmt, va_list ap)
{
    while (*fmt) {
        if (*fmt != '%') {
            print_char(*fmt++);
            continue;
        }
        fmt++;  /* skip '%' */

        /* Handle width for zero-padded hex */
        int width = 0;
        int zero_pad = 0;
        if (*fmt == '0') { zero_pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        /* Handle 'l' length modifier (ignored on 32-bit) */
        if (*fmt == 'l') fmt++;

        switch (*fmt) {
        case 'd':
        case 'i':
            print_int(va_arg(ap, int));
            break;
        case 'u':
            print_unsigned(va_arg(ap, unsigned), 10, 0);
            break;
        case 'x':
            if (width > 0 && zero_pad) {
                unsigned val = va_arg(ap, unsigned);
                char buf[9];
                int i;
                for (i = 0; i < 8; i++) {
                    unsigned d = (val >> ((7 - i) * 4)) & 0xF;
                    buf[i] = d < 10 ? '0' + d : 'a' + d - 10;
                }
                /* skip leading zeros but respect width */
                int start = 8 - width;
                if (start < 0) start = 0;
                for (i = start; i < 8; i++)
                    uart_putchar(buf[i]);
            } else {
                print_hex(va_arg(ap, unsigned));
            }
            break;
        case 'X':
            print_unsigned(va_arg(ap, unsigned), 16, 1);
            break;
        case 'p':
            print_pointer(va_arg(ap, unsigned));
            break;
        case 's':
            print_string(va_arg(ap, const char *));
            break;
        case 'c':
            print_char((char)va_arg(ap, int));
            break;
        case '%':
            uart_putchar('%');
            break;
        default:
            uart_putchar('%');
            print_char(*fmt);
            break;
        }
        fmt++;
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void kpanic(const char *fmt, ...)
{
    /* Disable interrupts */
    __asm__ volatile("csrc mstatus, %0" :: "r"(1 << 3));

    uart_puts("\r\n!!! KERNEL PANIC !!!\r\n");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    uart_puts("\r\nSystem halted.\r\n");

    while (1) __asm__ volatile("wfi");
    __builtin_unreachable();
}
