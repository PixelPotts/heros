#ifndef HAL_INPUT_H
#define HAL_INPUT_H

#include "hal.h"
#include "../kernel/input.h"

/* ── SDL-compatible scancodes (subset) ─────────────────────────── */
#define HAL_KEY_A           4
#define HAL_KEY_B           5
#define HAL_KEY_C           6
#define HAL_KEY_D           7
#define HAL_KEY_E           8
#define HAL_KEY_F           9
#define HAL_KEY_G           10
#define HAL_KEY_H           11
#define HAL_KEY_I           12
#define HAL_KEY_J           13
#define HAL_KEY_K           14
#define HAL_KEY_L           15
#define HAL_KEY_M           16
#define HAL_KEY_N           17
#define HAL_KEY_O           18
#define HAL_KEY_P           19
#define HAL_KEY_Q           20
#define HAL_KEY_R           21
#define HAL_KEY_S           22
#define HAL_KEY_T           23
#define HAL_KEY_U           24
#define HAL_KEY_V           25
#define HAL_KEY_W           26
#define HAL_KEY_X           27
#define HAL_KEY_Y           28
#define HAL_KEY_Z           29
#define HAL_KEY_1           30
#define HAL_KEY_2           31
#define HAL_KEY_3           32
#define HAL_KEY_4           33
#define HAL_KEY_5           34
#define HAL_KEY_6           35
#define HAL_KEY_7           36
#define HAL_KEY_8           37
#define HAL_KEY_9           38
#define HAL_KEY_0           39
#define HAL_KEY_RETURN      40
#define HAL_KEY_ESCAPE      41
#define HAL_KEY_BACKSPACE   42
#define HAL_KEY_TAB         43
#define HAL_KEY_SPACE       44
#define HAL_KEY_MINUS       45
#define HAL_KEY_EQUALS      46
#define HAL_KEY_LEFTBRACKET 47
#define HAL_KEY_RIGHTBRACKET 48
#define HAL_KEY_BACKSLASH   49
#define HAL_KEY_SEMICOLON   51
#define HAL_KEY_APOSTROPHE  52
#define HAL_KEY_GRAVE       53
#define HAL_KEY_COMMA       54
#define HAL_KEY_PERIOD      55
#define HAL_KEY_SLASH       56
#define HAL_KEY_F1          58
#define HAL_KEY_F2          59
#define HAL_KEY_F3          60
#define HAL_KEY_F4          61
#define HAL_KEY_F5          62
#define HAL_KEY_F6          63
#define HAL_KEY_F7          64
#define HAL_KEY_F8          65
#define HAL_KEY_F9          66
#define HAL_KEY_F10         67
#define HAL_KEY_F11         68
#define HAL_KEY_F12         69
#define HAL_KEY_DELETE      76
#define HAL_KEY_RIGHT       79
#define HAL_KEY_LEFT        80
#define HAL_KEY_DOWN        81
#define HAL_KEY_UP          82
#define HAL_KEY_HOME        74
#define HAL_KEY_END         77
#define HAL_KEY_PAGEUP      75
#define HAL_KEY_PAGEDOWN    78

/* Modifier flags */
#define HAL_MOD_LSHIFT   0x0001
#define HAL_MOD_RSHIFT   0x0002
#define HAL_MOD_LCTRL    0x0040
#define HAL_MOD_RCTRL    0x0080
#define HAL_MOD_LALT     0x0100
#define HAL_MOD_RALT     0x0200
#define HAL_MOD_SHIFT    (HAL_MOD_LSHIFT | HAL_MOD_RSHIFT)
#define HAL_MOD_CTRL     (HAL_MOD_LCTRL | HAL_MOD_RCTRL)
#define HAL_MOD_ALT      (HAL_MOD_LALT | HAL_MOD_RALT)

/* ── Event types ──────────────────────────────────────────────── */
typedef enum {
    HAL_EVT_NONE = 0,
    HAL_EVT_KEY_DOWN,
    HAL_EVT_KEY_UP,
    HAL_EVT_TEXT,
    HAL_EVT_MOUSE_MOVE,
    HAL_EVT_MOUSE_DOWN,
    HAL_EVT_MOUSE_UP,
    HAL_EVT_SCROLL
} hal_event_type_t;

typedef struct {
    hal_event_type_t type;
    uint16_t key;         /* scancode */
    uint16_t mod;         /* modifier flags */
    char     ch;          /* character for TEXT events */
    int      mouse_x;
    int      mouse_y;
    uint8_t  mouse_btn;   /* 1=left, 2=middle, 3=right */
    int      scroll_y;
} hal_event_t;

/* Poll for next input event. Returns 1 if event available, 0 if none. */
int  hal_input_poll(hal_event_t *evt);

/* Get current mouse position */
void hal_input_mouse_pos(int *x, int *y);

#endif
