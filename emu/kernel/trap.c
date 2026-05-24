#include "trap.h"
#include "timer.h"
#include "sched.h"
#include "syscall.h"
#include "kprintf.h"

void trap_dispatch(trap_frame_t *frame)
{
    uint32_t mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    if (mcause & 0x80000000) {
        /* ── Interrupt ─────────────────────────────────────────── */
        uint32_t code = mcause & 0x7FFFFFFF;

        switch (code) {
        case 7:  /* Machine timer interrupt */
            timer_set_next();
            sched_tick(frame);
            break;
        case 3:  /* Machine software interrupt */
            /* Clear software interrupt */
            (*(volatile uint32_t *)0x02000000) = 0;
            break;
        default:
            kprintf("[trap] Unknown interrupt: %u\n", (unsigned)code);
            break;
        }
    } else {
        /* ── Exception ─────────────────────────────────────────── */
        switch (mcause) {
        case CAUSE_ECALL_M:
            /* Advance past ecall BEFORE dispatch — sched_tick may
               swap the frame to a different task's saved state */
            frame->mepc += 4;
            syscall_dispatch(frame);
            break;

        case CAUSE_ILLEGAL_INSN: {
            uint32_t mtval;
            __asm__ volatile("csrr %0, mtval" : "=r"(mtval));
            kpanic("Illegal instruction at 0x%08x (insn=0x%08x)\n",
                   (unsigned)frame->mepc, (unsigned)mtval);
            break;
        }

        case CAUSE_LOAD_FAULT:
        case CAUSE_STORE_FAULT:
        case CAUSE_LOAD_MISALIGNED:
        case CAUSE_STORE_MISALIGNED: {
            uint32_t mtval;
            __asm__ volatile("csrr %0, mtval" : "=r"(mtval));
            kpanic("Memory fault at pc=0x%08x, addr=0x%08x, cause=%u\n",
                   (unsigned)frame->mepc, (unsigned)mtval, (unsigned)mcause);
            break;
        }

        default:
            kpanic("Unhandled exception: mcause=%u, mepc=0x%08x\n",
                   (unsigned)mcause, (unsigned)frame->mepc);
            break;
        }
    }
}
