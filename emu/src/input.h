#ifndef INPUT_H
#define INPUT_H

#include "emu.h"
#include <SDL2/SDL.h>

void     input_init(void);
uint32_t input_read(uint32_t offset);
void     input_write(uint32_t offset, uint32_t val);

/* Push SDL events into the input device ring buffers */
void input_push_key(uint32_t scancode, uint32_t mod, int down);
void input_push_text(char ch);
void input_push_mouse_move(int x, int y);
void input_push_mouse_button(int x, int y, uint8_t button, int down);
void input_push_scroll(int x, int y, int scroll_y);

#endif
