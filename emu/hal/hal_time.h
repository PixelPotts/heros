#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

uint32_t hal_get_ticks(void);    /* milliseconds since boot */
void     hal_delay(uint32_t ms); /* blocking delay */

#endif
