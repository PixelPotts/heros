#include "hal_input.h"

static int last_mouse_x = 0;
static int last_mouse_y = 0;
static uint16_t last_mod = 0;

int hal_input_poll(hal_event_t *evt)
{
    input_event_t raw;
    if (!input_driver_poll(&raw))
        return 0;

    switch (raw.type) {
    case INPUT_EVT_KEY_DOWN:
        evt->type = HAL_EVT_KEY_DOWN;
        evt->key = raw.key;
        evt->mod = raw.mod;
        last_mod = raw.mod;
        evt->ch = 0;
        evt->mouse_x = last_mouse_x;
        evt->mouse_y = last_mouse_y;
        evt->mouse_btn = 0;
        evt->scroll_y = 0;
        break;

    case INPUT_EVT_KEY_UP:
        evt->type = HAL_EVT_KEY_UP;
        evt->key = raw.key;
        evt->mod = raw.mod;
        last_mod = raw.mod;
        evt->ch = 0;
        evt->mouse_x = last_mouse_x;
        evt->mouse_y = last_mouse_y;
        evt->mouse_btn = 0;
        evt->scroll_y = 0;
        break;

    case INPUT_EVT_TEXT:
        evt->type = HAL_EVT_TEXT;
        evt->key = 0;
        evt->mod = 0;
        evt->ch = raw.ch;
        evt->mouse_x = last_mouse_x;
        evt->mouse_y = last_mouse_y;
        evt->mouse_btn = 0;
        evt->scroll_y = 0;
        break;

    case INPUT_EVT_MOUSE_MOVE:
        evt->type = HAL_EVT_MOUSE_MOVE;
        evt->key = 0;
        evt->mod = 0;
        evt->ch = 0;
        evt->mouse_x = raw.mouse_x;
        evt->mouse_y = raw.mouse_y;
        evt->mouse_btn = 0;
        evt->scroll_y = 0;
        last_mouse_x = raw.mouse_x;
        last_mouse_y = raw.mouse_y;
        break;

    case INPUT_EVT_MOUSE_DOWN:
        evt->type = HAL_EVT_MOUSE_DOWN;
        evt->key = 0;
        evt->mod = raw.mod;
        last_mod = raw.mod;
        evt->ch = 0;
        evt->mouse_x = raw.mouse_x;
        evt->mouse_y = raw.mouse_y;
        evt->mouse_btn = raw.mouse_btn;
        evt->scroll_y = 0;
        last_mouse_x = raw.mouse_x;
        last_mouse_y = raw.mouse_y;
        break;

    case INPUT_EVT_MOUSE_UP:
        evt->type = HAL_EVT_MOUSE_UP;
        evt->key = 0;
        evt->mod = raw.mod;
        last_mod = raw.mod;
        evt->ch = 0;
        evt->mouse_x = raw.mouse_x;
        evt->mouse_y = raw.mouse_y;
        evt->mouse_btn = raw.mouse_btn;
        evt->scroll_y = 0;
        last_mouse_x = raw.mouse_x;
        last_mouse_y = raw.mouse_y;
        break;

    case INPUT_EVT_SCROLL:
        evt->type = HAL_EVT_SCROLL;
        evt->key = 0;
        evt->mod = 0;
        evt->ch = 0;
        evt->mouse_x = raw.mouse_x;
        evt->mouse_y = raw.mouse_y;
        evt->mouse_btn = 0;
        evt->scroll_y = raw.scroll_y;
        break;

    default:
        return 0;
    }

    return 1;
}

void hal_input_mouse_pos(int *x, int *y)
{
    *x = last_mouse_x;
    *y = last_mouse_y;
}

uint16_t hal_input_get_mod(void)
{
    return last_mod;
}
