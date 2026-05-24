#ifndef KERNEL_INPUT_H
#define KERNEL_INPUT_H

#include <stdint.h>

/* Input event types */
#define INPUT_EVT_NONE        0
#define INPUT_EVT_KEY_DOWN    1
#define INPUT_EVT_KEY_UP      2
#define INPUT_EVT_TEXT        3
#define INPUT_EVT_MOUSE_MOVE  4
#define INPUT_EVT_MOUSE_DOWN  5
#define INPUT_EVT_MOUSE_UP    6
#define INPUT_EVT_SCROLL      7

typedef struct {
    uint8_t  type;
    uint16_t key;         /* scancode for key events */
    uint16_t mod;         /* modifier flags */
    char     ch;          /* character for text events */
    int16_t  mouse_x;    /* absolute position for mouse events */
    int16_t  mouse_y;
    uint8_t  mouse_btn;  /* button for mouse down/up */
    int8_t   scroll_y;   /* scroll delta */
} input_event_t;

void input_driver_init(void);
int  input_driver_poll(void *event_out);

#endif
