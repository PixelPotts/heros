#ifndef CPU_H
#define CPU_H

#include "emu.h"

typedef struct {
    uint32_t x[32];       /* general-purpose registers */
    uint32_t pc;

    /* CSRs */
    uint32_t mstatus;
    uint32_t mie;
    uint32_t mip;
    uint32_t mtvec;
    uint32_t mepc;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t mscratch;

    uint64_t insn_count;
    bool     halted;

    /* Cached RAM pointer for fast fetch (set during init) */
    uint8_t *ram_ptr;
} cpu_t;

void cpu_init(cpu_t *cpu, uint32_t entry);
void cpu_step(cpu_t *cpu);
void cpu_check_interrupts(cpu_t *cpu);

#endif
