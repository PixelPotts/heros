#ifndef KERNEL_KPRINTF_H
#define KERNEL_KPRINTF_H

#include <stdarg.h>

void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);
void kpanic(const char *fmt, ...) __attribute__((format(printf, 1, 2), noreturn));

#endif
