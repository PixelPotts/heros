#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <stdint.h>

#define TIMER_FREQ_HZ     10000000   /* CLINT ticks per second (10 MHz) */
#define TIMER_QUANTUM_MS  10         /* scheduler quantum */

void     timer_init(void);
void     timer_set_next(void);
uint64_t timer_get_ticks(void);
uint32_t timer_uptime_ms(void);

#endif
