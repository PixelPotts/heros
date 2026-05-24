#ifndef UI_LOCKSCREEN_H
#define UI_LOCKSCREEN_H

#include "types.h"

void lockscreen_init(void);
void lockscreen_render(void);
int  lockscreen_handle_key(uint16_t key, uint16_t mod);
int  lockscreen_handle_text(char ch);
int  lockscreen_is_locked(void);
void lockscreen_lock(void);

#endif
