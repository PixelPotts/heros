#ifndef KERNEL_TRAP_H
#define KERNEL_TRAP_H

#include <stdint.h>

/* Trap frame layout — must match boot.S save order */
typedef struct {
    uint32_t ra;        /* x1  - offset 0   */
    uint32_t sp;        /* x2  - offset 4   */
    uint32_t gp;        /* x3  - offset 8   */
    uint32_t tp;        /* x4  - offset 12  */
    uint32_t t0;        /* x5  - offset 16  */
    uint32_t t1;        /* x6  - offset 20  */
    uint32_t t2;        /* x7  - offset 24  */
    uint32_t s0;        /* x8  - offset 28  */
    uint32_t s1;        /* x9  - offset 32  */
    uint32_t a0;        /* x10 - offset 36  */
    uint32_t a1;        /* x11 - offset 40  */
    uint32_t a2;        /* x12 - offset 44  */
    uint32_t a3;        /* x13 - offset 48  */
    uint32_t a4;        /* x14 - offset 52  */
    uint32_t a5;        /* x15 - offset 56  */
    uint32_t a6;        /* x16 - offset 60  */
    uint32_t a7;        /* x17 - offset 64  */
    uint32_t s2;        /* x18 - offset 68  */
    uint32_t s3;        /* x19 - offset 72  */
    uint32_t s4;        /* x20 - offset 76  */
    uint32_t s5;        /* x21 - offset 80  */
    uint32_t s6;        /* x22 - offset 84  */
    uint32_t s7;        /* x23 - offset 88  */
    uint32_t s8;        /* x24 - offset 92  */
    uint32_t s9;        /* x25 - offset 96  */
    uint32_t s10;       /* x26 - offset 100 */
    uint32_t s11;       /* x27 - offset 104 */
    uint32_t t3;        /* x28 - offset 108 */
    uint32_t t4;        /* x29 - offset 112 */
    uint32_t t5;        /* x30 - offset 116 */
    uint32_t t6;        /* x31 - offset 120 */
    uint32_t mepc;      /* saved PC - offset 124 */
} trap_frame_t;

/* RISC-V exception/interrupt causes */
#define CAUSE_INSN_MISALIGNED   0
#define CAUSE_ILLEGAL_INSN      2
#define CAUSE_BREAKPOINT        3
#define CAUSE_LOAD_MISALIGNED   4
#define CAUSE_LOAD_FAULT        5
#define CAUSE_STORE_MISALIGNED  6
#define CAUSE_STORE_FAULT       7
#define CAUSE_ECALL_M           11

#define CAUSE_M_SOFTWARE_INT    (0x80000000 | 3)
#define CAUSE_M_TIMER_INT       (0x80000000 | 7)

/* Called from boot.S _trap_entry */
void trap_dispatch(trap_frame_t *frame);

#endif
