.section .text.init
.global _start

_start:
    /* Set up stack pointer at top of RAM (128 MB from 0x80000000) */
    li   sp, 0x87FF0000

    /* Set up trap vector */
    la   t0, _trap_entry
    csrw mtvec, t0

    /* Jump to C entry point */
    call kernel_main

    /* If kernel_main returns, spin forever */
1:  wfi
    j    1b

.align 4
_trap_entry:
    /* Save registers we'll use */
    addi sp, sp, -64
    sw   ra,  0(sp)
    sw   t0,  4(sp)
    sw   t1,  8(sp)
    sw   t2, 12(sp)
    sw   a0, 16(sp)
    sw   a1, 20(sp)
    sw   a2, 24(sp)
    sw   a3, 28(sp)

    /* Call C trap handler */
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval
    call trap_handler

    /* trap_handler returns the new mepc */
    csrw mepc, a0

    /* Restore registers */
    lw   ra,  0(sp)
    lw   t0,  4(sp)
    lw   t1,  8(sp)
    lw   t2, 12(sp)
    lw   a0, 16(sp)
    lw   a1, 20(sp)
    lw   a2, 24(sp)
    lw   a3, 28(sp)
    addi sp, sp, 64

    mret
