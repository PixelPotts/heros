.section .text.init
.global _start

_start:
    /* ── Set stack pointer ─────────────────────────────────────── */
    li   sp, 0x87FF0000

    /* ── Zero BSS ──────────────────────────────────────────────── */
    la   t0, __bss_start
    la   t1, __bss_end
1:  bge  t0, t1, 2f
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    1b
2:

    /* ── Set trap vector ───────────────────────────────────────── */
    la   t0, _trap_entry
    csrw mtvec, t0

    /* ── Set mscratch to point to a scratch area for trap frame ─ */
    la   t0, _trap_scratch
    csrw mscratch, t0

    /* ── Jump to C entry point ─────────────────────────────────── */
    call kmain

    /* If kmain returns, spin forever */
3:  wfi
    j    3b

/* ═══════════════════════════════════════════════════════════════════
   Trap entry — saves all 31 GPRs via mscratch swap pattern
   ═══════════════════════════════════════════════════════════════════ */
.align 4
.global _trap_entry
_trap_entry:
    /* Swap sp with mscratch (mscratch holds trap frame pointer) */
    csrrw sp, mscratch, sp

    /* Save all 31 general-purpose registers into trap frame */
    sw   x1,   0(sp)     /* ra */
    /* x2 (sp) was swapped into mscratch — save it below */
    sw   x3,   8(sp)     /* gp */
    sw   x4,  12(sp)     /* tp */
    sw   x5,  16(sp)     /* t0 */
    sw   x6,  20(sp)     /* t1 */
    sw   x7,  24(sp)     /* t2 */
    sw   x8,  28(sp)     /* s0/fp */
    sw   x9,  32(sp)     /* s1 */
    sw   x10, 36(sp)     /* a0 */
    sw   x11, 40(sp)     /* a1 */
    sw   x12, 44(sp)     /* a2 */
    sw   x13, 48(sp)     /* a3 */
    sw   x14, 52(sp)     /* a4 */
    sw   x15, 56(sp)     /* a5 */
    sw   x16, 60(sp)     /* a6 */
    sw   x17, 64(sp)     /* a7 */
    sw   x18, 68(sp)     /* s2 */
    sw   x19, 72(sp)     /* s3 */
    sw   x20, 76(sp)     /* s4 */
    sw   x21, 80(sp)     /* s5 */
    sw   x22, 84(sp)     /* s6 */
    sw   x23, 88(sp)     /* s7 */
    sw   x24, 92(sp)     /* s8 */
    sw   x25, 96(sp)     /* s9 */
    sw   x26, 100(sp)    /* s10 */
    sw   x27, 104(sp)    /* s11 */
    sw   x28, 108(sp)    /* t3 */
    sw   x29, 112(sp)    /* t4 */
    sw   x30, 116(sp)    /* t5 */
    sw   x31, 120(sp)    /* t6 */

    /* Save the original sp (now in mscratch) */
    csrr t0, mscratch
    sw   t0,  4(sp)      /* x2 = sp */

    /* Save mepc */
    csrr t0, mepc
    sw   t0, 124(sp)

    /* Call C trap handler: trap_dispatch(trap_frame_t *frame) */
    mv   a0, sp
    call trap_dispatch

    /* Restore mepc (trap handler may have modified it) */
    lw   t0, 124(sp)
    csrw mepc, t0

    /* Restore original sp into mscratch for next trap */
    lw   t0,  4(sp)
    csrw mscratch, t0

    /* Restore all GPRs */
    lw   x1,   0(sp)
    /* x2 (sp) restored last */
    lw   x3,   8(sp)
    lw   x4,  12(sp)
    lw   x5,  16(sp)
    lw   x6,  20(sp)
    lw   x7,  24(sp)
    lw   x8,  28(sp)
    lw   x9,  32(sp)
    lw   x10, 36(sp)
    lw   x11, 40(sp)
    lw   x12, 44(sp)
    lw   x13, 48(sp)
    lw   x14, 52(sp)
    lw   x15, 56(sp)
    lw   x16, 60(sp)
    lw   x17, 64(sp)
    lw   x18, 68(sp)
    lw   x19, 72(sp)
    lw   x20, 76(sp)
    lw   x21, 80(sp)
    lw   x22, 84(sp)
    lw   x23, 88(sp)
    lw   x24, 92(sp)
    lw   x25, 96(sp)
    lw   x26, 100(sp)
    lw   x27, 104(sp)
    lw   x28, 108(sp)
    lw   x29, 112(sp)
    lw   x30, 116(sp)
    lw   x31, 120(sp)

    /* Restore sp from mscratch */
    csrrw sp, mscratch, sp

    mret

/* ── Trap scratch area (128 bytes for 31 regs + mepc) ──────────── */
.section .bss
.align 4
.global _trap_scratch
_trap_scratch:
    .space 128
