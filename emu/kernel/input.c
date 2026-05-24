#include "input.h"
#include "kprintf.h"

/* Input device MMIO registers */
#define INPUT_BASE          0x10002000
#define INPUT_STATUS        (*(volatile uint32_t *)(INPUT_BASE + 0x00))
#define INPUT_KBD_KEY       (*(volatile uint32_t *)(INPUT_BASE + 0x04))
#define INPUT_KBD_MOD       (*(volatile uint32_t *)(INPUT_BASE + 0x08))
#define INPUT_KBD_STATE     (*(volatile uint32_t *)(INPUT_BASE + 0x0C))
#define INPUT_KBD_CHAR      (*(volatile uint32_t *)(INPUT_BASE + 0x10))
#define INPUT_KBD_ACK       (*(volatile uint32_t *)(INPUT_BASE + 0x14))
#define INPUT_MOUSE_X       (*(volatile uint32_t *)(INPUT_BASE + 0x20))
#define INPUT_MOUSE_Y       (*(volatile uint32_t *)(INPUT_BASE + 0x24))
#define INPUT_MOUSE_BTN     (*(volatile uint32_t *)(INPUT_BASE + 0x28))
#define INPUT_MOUSE_STATE   (*(volatile uint32_t *)(INPUT_BASE + 0x2C))
#define INPUT_MOUSE_SCROLL  (*(volatile uint32_t *)(INPUT_BASE + 0x34))
#define INPUT_MOUSE_ACK     (*(volatile uint32_t *)(INPUT_BASE + 0x38))

/* Status bits */
#define STATUS_KBD_READY    (1 << 0)
#define STATUS_MOUSE_READY  (1 << 1)
#define STATUS_TEXT_READY   (1 << 2)
#define STATUS_SCROLL_READY (1 << 3)

void input_driver_init(void)
{
    kprintf("[input] Input device initialized\n");
}

int input_driver_poll(void *event_out)
{
    input_event_t *evt = (input_event_t *)event_out;
    uint32_t status = INPUT_STATUS;

    if (status & STATUS_KBD_READY) {
        uint32_t key   = INPUT_KBD_KEY;
        uint32_t mod   = INPUT_KBD_MOD;
        uint32_t state = INPUT_KBD_STATE;
        INPUT_KBD_ACK = 1;

        evt->type = (state == 1) ? INPUT_EVT_KEY_DOWN : INPUT_EVT_KEY_UP;
        evt->key = (uint16_t)key;
        evt->mod = (uint16_t)mod;
        evt->ch = 0;
        evt->mouse_x = 0;
        evt->mouse_y = 0;
        evt->mouse_btn = 0;
        evt->scroll_y = 0;
        return 1;
    }

    if (status & STATUS_TEXT_READY) {
        uint32_t ch = INPUT_KBD_CHAR;
        INPUT_KBD_ACK = 1;

        evt->type = INPUT_EVT_TEXT;
        evt->key = 0;
        evt->mod = 0;
        evt->ch = (char)ch;
        evt->mouse_x = 0;
        evt->mouse_y = 0;
        evt->mouse_btn = 0;
        evt->scroll_y = 0;
        return 1;
    }

    if (status & STATUS_MOUSE_READY) {
        uint32_t x   = INPUT_MOUSE_X;
        uint32_t y   = INPUT_MOUSE_Y;
        uint32_t btn = INPUT_MOUSE_BTN;
        uint32_t ms  = INPUT_MOUSE_STATE;
        uint32_t mod = INPUT_KBD_MOD;  /* mod state available for mouse events too */
        INPUT_MOUSE_ACK = 1;

        if (ms == 1)      evt->type = INPUT_EVT_MOUSE_DOWN;
        else if (ms == 2) evt->type = INPUT_EVT_MOUSE_UP;
        else              evt->type = INPUT_EVT_MOUSE_MOVE;

        evt->key = 0;
        evt->mod = (uint16_t)mod;
        evt->ch = 0;
        evt->mouse_x = (int16_t)x;
        evt->mouse_y = (int16_t)y;
        evt->mouse_btn = (uint8_t)btn;
        evt->scroll_y = 0;
        return 1;
    }

    if (status & STATUS_SCROLL_READY) {
        int32_t scroll = (int32_t)INPUT_MOUSE_SCROLL;
        uint32_t x = INPUT_MOUSE_X;
        uint32_t y = INPUT_MOUSE_Y;
        INPUT_MOUSE_ACK = 1;

        evt->type = INPUT_EVT_SCROLL;
        evt->key = 0;
        evt->mod = 0;
        evt->ch = 0;
        evt->mouse_x = (int16_t)x;
        evt->mouse_y = (int16_t)y;
        evt->mouse_btn = 0;
        evt->scroll_y = (int8_t)scroll;
        return 1;
    }

    return 0;
}
