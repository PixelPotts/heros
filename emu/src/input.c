#include "input.h"
#include <string.h>

/* ── Ring buffer for events ──────────────────────────────────────── */
#define RING_SIZE   64

typedef struct {
    uint32_t type;      /* 1=key, 2=text, 3=mouse, 4=scroll */
    uint32_t key;
    uint32_t mod;
    uint32_t state;     /* key: 1=down,0=up; mouse: 1=down,2=up,0=move */
    uint32_t ch;        /* text character */
    uint32_t mouse_x;
    uint32_t mouse_y;
    uint32_t mouse_btn;
    int32_t  scroll_y;
} input_evt_t;

static input_evt_t ring[RING_SIZE];
static int ring_head = 0;
static int ring_tail = 0;

/* Current event being read by guest */
static input_evt_t current;
static int current_valid = 0;

/* Current mouse position (always available) */
static uint32_t cur_mouse_x = 0;
static uint32_t cur_mouse_y = 0;

/* Status register bits */
#define STATUS_KBD_READY    (1 << 0)
#define STATUS_MOUSE_READY  (1 << 1)
#define STATUS_TEXT_READY   (1 << 2)
#define STATUS_SCROLL_READY (1 << 3)

static int ring_empty(void) { return ring_head == ring_tail; }
static int ring_full(void) { return ((ring_head + 1) % RING_SIZE) == ring_tail; }

static void ring_push(const input_evt_t *evt)
{
    if (ring_full()) return;  /* drop if full */
    ring[ring_head] = *evt;
    ring_head = (ring_head + 1) % RING_SIZE;
}

static int ring_pop(input_evt_t *evt)
{
    if (ring_empty()) return 0;
    *evt = ring[ring_tail];
    ring_tail = (ring_tail + 1) % RING_SIZE;
    return 1;
}

static void load_next_event(void)
{
    if (ring_pop(&current))
        current_valid = 1;
    else
        current_valid = 0;
}

void input_init(void)
{
    memset(ring, 0, sizeof(ring));
    ring_head = ring_tail = 0;
    current_valid = 0;
    cur_mouse_x = cur_mouse_y = 0;
}

/* ── MMIO register offsets ───────────────────────────────────────── */
#define OFF_STATUS      0x00
#define OFF_KBD_KEY     0x04
#define OFF_KBD_MOD     0x08
#define OFF_KBD_STATE   0x0C
#define OFF_KBD_CHAR    0x10
#define OFF_KBD_ACK     0x14
#define OFF_MOUSE_X     0x20
#define OFF_MOUSE_Y     0x24
#define OFF_MOUSE_BTN   0x28
#define OFF_MOUSE_STATE 0x2C
#define OFF_MOUSE_SCROLL 0x34
#define OFF_MOUSE_ACK   0x38

uint32_t input_read(uint32_t offset)
{
    /* If no current event, try to load one */
    if (!current_valid)
        load_next_event();

    switch (offset) {
    case OFF_STATUS: {
        if (!current_valid)
            load_next_event();
        if (!current_valid) return 0;

        uint32_t status = 0;
        if (current.type == 1) status |= STATUS_KBD_READY;
        if (current.type == 2) status |= STATUS_TEXT_READY;
        if (current.type == 3) status |= STATUS_MOUSE_READY;
        if (current.type == 4) status |= STATUS_SCROLL_READY;
        return status;
    }
    case OFF_KBD_KEY:     return current_valid ? current.key : 0;
    case OFF_KBD_MOD:     return current_valid ? current.mod : 0;
    case OFF_KBD_STATE:   return current_valid ? current.state : 0;
    case OFF_KBD_CHAR:    return current_valid ? current.ch : 0;
    case OFF_MOUSE_X:     return cur_mouse_x;
    case OFF_MOUSE_Y:     return cur_mouse_y;
    case OFF_MOUSE_BTN:   return current_valid ? current.mouse_btn : 0;
    case OFF_MOUSE_STATE: return current_valid ? current.state : 0;
    case OFF_MOUSE_SCROLL: return current_valid ? (uint32_t)current.scroll_y : 0;
    default: return 0;
    }
}

void input_write(uint32_t offset, uint32_t val)
{
    (void)val;
    /* ACK — consume current event and load next */
    if (offset == OFF_KBD_ACK || offset == OFF_MOUSE_ACK) {
        current_valid = 0;
        load_next_event();
    }
}

/* ── Push functions called from SDL event loop ───────────────────── */

void input_push_key(uint32_t scancode, uint32_t mod, int down)
{
    input_evt_t evt = {0};
    evt.type = 1;
    evt.key = scancode;
    evt.mod = mod;
    evt.state = down ? 1 : 0;
    ring_push(&evt);
}

void input_push_text(char ch)
{
    input_evt_t evt = {0};
    evt.type = 2;
    evt.ch = (uint32_t)(uint8_t)ch;
    ring_push(&evt);
}

void input_push_mouse_move(int x, int y)
{
    /* Scale from window coords to internal FB coords */
    cur_mouse_x = (uint32_t)x;
    cur_mouse_y = (uint32_t)y;

    input_evt_t evt = {0};
    evt.type = 3;
    evt.state = 0;  /* move */
    evt.mouse_x = (uint32_t)x;
    evt.mouse_y = (uint32_t)y;
    ring_push(&evt);
}

void input_push_mouse_button(int x, int y, uint8_t button, int down)
{
    cur_mouse_x = (uint32_t)x;
    cur_mouse_y = (uint32_t)y;

    input_evt_t evt = {0};
    evt.type = 3;
    evt.state = down ? 1 : 2;
    evt.mouse_x = (uint32_t)x;
    evt.mouse_y = (uint32_t)y;
    evt.mouse_btn = button;
    ring_push(&evt);
}

void input_push_scroll(int x, int y, int scroll_y)
{
    input_evt_t evt = {0};
    evt.type = 4;
    evt.mouse_x = (uint32_t)x;
    evt.mouse_y = (uint32_t)y;
    evt.scroll_y = (int32_t)scroll_y;
    ring_push(&evt);
}
