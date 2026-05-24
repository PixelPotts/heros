#include "hal_time.h"
#include "../kernel/timer.h"
#include "../kernel/sched.h"

uint32_t hal_get_ticks(void)
{
    return timer_uptime_ms();
}

void hal_delay(uint32_t ms)
{
    sched_sleep_ms(ms);
}
