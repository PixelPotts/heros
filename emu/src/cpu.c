#include "cpu.h"
#include "bus.h"
#include <stdio.h>
#include <stdlib.h>

/* ── RV32I opcode field constants ───────────────────────────────── */
#define OP_LUI       0x37
#define OP_AUIPC     0x17
#define OP_JAL       0x6F
#define OP_JALR      0x67
#define OP_BRANCH    0x63
#define OP_LOAD      0x03
#define OP_STORE     0x23
#define OP_OP_IMM    0x13
#define OP_OP        0x33
#define OP_FENCE     0x0F
#define OP_SYSTEM    0x73

/* ── funct3 for branches ────────────────────────────────────────── */
#define F3_BEQ   0
#define F3_BNE   1
#define F3_BLT   4
#define F3_BGE   5
#define F3_BLTU  6
#define F3_BGEU  7

/* ── funct3 for loads ───────────────────────────────────────────── */
#define F3_LB    0
#define F3_LH    1
#define F3_LW    2
#define F3_LBU   4
#define F3_LHU   5

/* ── funct3 for stores ──────────────────────────────────────────── */
#define F3_SB    0
#define F3_SH    1
#define F3_SW    2

/* ── funct3 for ALU ops ─────────────────────────────────────────── */
#define F3_ADD   0   /* also SUB (funct7 bit) */
#define F3_SLL   1
#define F3_SLT   2
#define F3_SLTU  3
#define F3_XOR   4
#define F3_SRL   5   /* also SRA */
#define F3_OR    6
#define F3_AND   7

/* ── funct3 for SYSTEM ──────────────────────────────────────────── */
#define F3_ECALL   0   /* ECALL/EBREAK/MRET */
#define F3_CSRRW   1
#define F3_CSRRS   2
#define F3_CSRRC   3
#define F3_CSRRWI  5
#define F3_CSRRSI  6
#define F3_CSRRCI  7

/* ── Helpers ────────────────────────────────────────────────────── */

static inline uint32_t sign_extend(uint32_t val, int bits)
{
    uint32_t mask = 1U << (bits - 1);
    return (val ^ mask) - mask;
}

static inline uint32_t extract(uint32_t insn, int lo, int len)
{
    return (insn >> lo) & ((1U << len) - 1);
}

/* ── CSR read / write ───────────────────────────────────────────── */

static uint32_t csr_read(cpu_t *cpu, uint32_t addr)
{
    switch (addr) {
    case CSR_MSTATUS:  return cpu->mstatus;
    case CSR_MIE:      return cpu->mie;
    case CSR_MTVEC:    return cpu->mtvec;
    case CSR_MSCRATCH: return cpu->mscratch;
    case CSR_MEPC:     return cpu->mepc;
    case CSR_MCAUSE:   return cpu->mcause;
    case CSR_MTVAL:    return cpu->mtval;
    case CSR_MIP:      return cpu->mip;
    case CSR_MHARTID:  return 0;
    default:           return 0;
    }
}

static void csr_write(cpu_t *cpu, uint32_t addr, uint32_t val)
{
    switch (addr) {
    case CSR_MSTATUS:  cpu->mstatus  = val; break;
    case CSR_MIE:      cpu->mie      = val; break;
    case CSR_MTVEC:    cpu->mtvec    = val; break;
    case CSR_MSCRATCH: cpu->mscratch = val; break;
    case CSR_MEPC:     cpu->mepc     = val; break;
    case CSR_MCAUSE:   cpu->mcause   = val; break;
    case CSR_MTVAL:    cpu->mtval    = val; break;
    case CSR_MIP:      cpu->mip      = val; break;
    default:           break;
    }
}

/* ── Trap entry ─────────────────────────────────────────────────── */

static void trap(cpu_t *cpu, uint32_t cause, uint32_t tval)
{
    cpu->mepc    = cpu->pc;   /* pc of faulting / interrupted insn */
    cpu->mcause  = cause;
    cpu->mtval   = tval;

    /* Save MIE to MPIE, clear MIE */
    if (cpu->mstatus & MSTATUS_MIE)
        cpu->mstatus |= MSTATUS_MPIE;
    else
        cpu->mstatus &= ~MSTATUS_MPIE;
    cpu->mstatus &= ~MSTATUS_MIE;

    /* Jump to trap vector */
    cpu->pc = cpu->mtvec & ~3U;   /* ignore mode bits for now (direct) */
}

/* ── Interrupt check (called externally) ────────────────────────── */

void cpu_check_interrupts(cpu_t *cpu)
{
    if (!(cpu->mstatus & MSTATUS_MIE))
        return;   /* interrupts globally disabled */

    /* Timer interrupt */
    if ((cpu->mip & MIP_MTIP) && (cpu->mie & MIP_MTIP)) {
        trap(cpu, CAUSE_M_TIMER_INT, 0);
        cpu->mip &= ~MIP_MTIP;   /* clear pending */
        return;
    }

    /* Software interrupt */
    if ((cpu->mip & MIP_MSIP) && (cpu->mie & MIP_MSIP)) {
        trap(cpu, CAUSE_M_SOFTWARE_INT, 0);
        cpu->mip &= ~MIP_MSIP;
        return;
    }
}

/* ── Init ───────────────────────────────────────────────────────── */

void cpu_init(cpu_t *cpu, uint32_t entry)
{
    for (int i = 0; i < 32; i++)
        cpu->x[i] = 0;
    cpu->pc       = entry;
    cpu->mstatus  = 0;
    cpu->mie      = 0;
    cpu->mip      = 0;
    cpu->mtvec    = 0;
    cpu->mepc     = 0;
    cpu->mcause   = 0;
    cpu->mtval    = 0;
    cpu->mscratch = 0;
    cpu->insn_count = 0;
    cpu->halted   = false;
}

/* ── Execute one instruction ────────────────────────────────────── */

void cpu_step(cpu_t *cpu)
{
    if (cpu->halted) return;

    bus_result_t br;
    uint32_t insn = bus_read32(cpu->pc, &br);
    if (br.exception) {
        trap(cpu, CAUSE_LOAD_FAULT, cpu->pc);
        return;
    }

    uint32_t opcode = insn & 0x7F;
    uint32_t rd     = extract(insn, 7, 5);
    uint32_t funct3 = extract(insn, 12, 3);
    uint32_t rs1    = extract(insn, 15, 5);
    uint32_t rs2    = extract(insn, 20, 5);
    uint32_t funct7 = extract(insn, 25, 7);

    uint32_t next_pc = cpu->pc + 4;

    switch (opcode) {

    /* ── LUI ────────────────────────────────────────────────────── */
    case OP_LUI:
        if (rd) cpu->x[rd] = insn & 0xFFFFF000;
        break;

    /* ── AUIPC ──────────────────────────────────────────────────── */
    case OP_AUIPC:
        if (rd) cpu->x[rd] = cpu->pc + (insn & 0xFFFFF000);
        break;

    /* ── JAL ────────────────────────────────────────────────────── */
    case OP_JAL: {
        int32_t imm = (int32_t)(
              ((insn >> 31) << 20)
            | (extract(insn, 12, 8) << 12)
            | (extract(insn, 20, 1) << 11)
            | (extract(insn, 21, 10) << 1)
        );
        imm = (int32_t)sign_extend(imm, 21);
        if (rd) cpu->x[rd] = next_pc;
        next_pc = cpu->pc + imm;
        break;
    }

    /* ── JALR ───────────────────────────────────────────────────── */
    case OP_JALR: {
        int32_t imm = (int32_t)sign_extend(insn >> 20, 12);
        uint32_t target = (cpu->x[rs1] + imm) & ~1U;
        if (rd) cpu->x[rd] = next_pc;
        next_pc = target;
        break;
    }

    /* ── BRANCH ─────────────────────────────────────────────────── */
    case OP_BRANCH: {
        int32_t imm = (int32_t)(
              ((insn >> 31) << 12)
            | (extract(insn, 7, 1) << 11)
            | (extract(insn, 25, 6) << 5)
            | (extract(insn, 8, 4) << 1)
        );
        imm = (int32_t)sign_extend(imm, 13);
        int32_t  a = (int32_t)cpu->x[rs1];
        int32_t  b = (int32_t)cpu->x[rs2];
        uint32_t ua = cpu->x[rs1];
        uint32_t ub = cpu->x[rs2];
        bool take = false;

        switch (funct3) {
        case F3_BEQ:  take = (ua == ub);   break;
        case F3_BNE:  take = (ua != ub);   break;
        case F3_BLT:  take = (a < b);     break;
        case F3_BGE:  take = (a >= b);    break;
        case F3_BLTU: take = (ua < ub);   break;
        case F3_BGEU: take = (ua >= ub);  break;
        default:
            trap(cpu, CAUSE_ILLEGAL_INSN, insn);
            return;
        }
        if (take) next_pc = cpu->pc + imm;
        break;
    }

    /* ── LOAD ───────────────────────────────────────────────────── */
    case OP_LOAD: {
        int32_t imm = (int32_t)sign_extend(insn >> 20, 12);
        uint32_t addr = cpu->x[rs1] + imm;

        switch (funct3) {
        case F3_LB: {
            uint8_t val = bus_read8(addr, &br);
            if (br.exception) { trap(cpu, br.exception, addr); return; }
            if (rd) cpu->x[rd] = (uint32_t)(int32_t)(int8_t)val;
            break;
        }
        case F3_LH: {
            uint16_t val = bus_read16(addr, &br);
            if (br.exception) { trap(cpu, br.exception, addr); return; }
            if (rd) cpu->x[rd] = (uint32_t)(int32_t)(int16_t)val;
            break;
        }
        case F3_LW: {
            uint32_t val = bus_read32(addr, &br);
            if (br.exception) { trap(cpu, br.exception, addr); return; }
            if (rd) cpu->x[rd] = val;
            break;
        }
        case F3_LBU: {
            uint8_t val = bus_read8(addr, &br);
            if (br.exception) { trap(cpu, br.exception, addr); return; }
            if (rd) cpu->x[rd] = val;
            break;
        }
        case F3_LHU: {
            uint16_t val = bus_read16(addr, &br);
            if (br.exception) { trap(cpu, br.exception, addr); return; }
            if (rd) cpu->x[rd] = val;
            break;
        }
        default:
            trap(cpu, CAUSE_ILLEGAL_INSN, insn);
            return;
        }
        break;
    }

    /* ── STORE ──────────────────────────────────────────────────── */
    case OP_STORE: {
        int32_t imm = (int32_t)sign_extend(
            (extract(insn, 25, 7) << 5) | extract(insn, 7, 5), 12);
        uint32_t addr = cpu->x[rs1] + imm;
        uint32_t val  = cpu->x[rs2];

        switch (funct3) {
        case F3_SB:
            bus_write8(addr, (uint8_t)val, &br);
            break;
        case F3_SH:
            bus_write16(addr, (uint16_t)val, &br);
            break;
        case F3_SW:
            bus_write32(addr, val, &br);
            break;
        default:
            trap(cpu, CAUSE_ILLEGAL_INSN, insn);
            return;
        }
        if (br.exception) { trap(cpu, br.exception, addr); return; }
        break;
    }

    /* ── OP-IMM (register-immediate ALU) ────────────────────────── */
    case OP_OP_IMM: {
        int32_t imm = (int32_t)sign_extend(insn >> 20, 12);
        uint32_t src = cpu->x[rs1];
        uint32_t result = 0;
        uint32_t shamt = imm & 0x1F;

        switch (funct3) {
        case F3_ADD:  result = src + (uint32_t)imm;                   break;
        case F3_SLT:  result = ((int32_t)src < imm) ? 1 : 0;         break;
        case F3_SLTU: result = (src < (uint32_t)imm) ? 1 : 0;        break;
        case F3_XOR:  result = src ^ (uint32_t)imm;                   break;
        case F3_OR:   result = src | (uint32_t)imm;                   break;
        case F3_AND:  result = src & (uint32_t)imm;                   break;
        case F3_SLL:  result = src << shamt;                           break;
        case F3_SRL:
            if (funct7 & 0x20)
                result = (uint32_t)((int32_t)src >> shamt);  /* SRAI */
            else
                result = src >> shamt;                         /* SRLI */
            break;
        default:
            trap(cpu, CAUSE_ILLEGAL_INSN, insn);
            return;
        }
        if (rd) cpu->x[rd] = result;
        break;
    }

    /* ── OP (register-register ALU) ─────────────────────────────── */
    case OP_OP: {
        uint32_t a = cpu->x[rs1];
        uint32_t b = cpu->x[rs2];
        uint32_t result = 0;

        switch (funct3) {
        case F3_ADD:
            result = (funct7 & 0x20) ? (a - b) : (a + b);
            break;
        case F3_SLL:  result = a << (b & 0x1F);                       break;
        case F3_SLT:  result = ((int32_t)a < (int32_t)b) ? 1 : 0;    break;
        case F3_SLTU: result = (a < b) ? 1 : 0;                       break;
        case F3_XOR:  result = a ^ b;                                  break;
        case F3_SRL:
            if (funct7 & 0x20)
                result = (uint32_t)((int32_t)a >> (b & 0x1F));  /* SRA */
            else
                result = a >> (b & 0x1F);                        /* SRL */
            break;
        case F3_OR:   result = a | b;                                  break;
        case F3_AND:  result = a & b;                                  break;
        default:
            trap(cpu, CAUSE_ILLEGAL_INSN, insn);
            return;
        }
        if (rd) cpu->x[rd] = result;
        break;
    }

    /* ── FENCE — treat as NOP ───────────────────────────────────── */
    case OP_FENCE:
        break;

    /* ── SYSTEM (ECALL, EBREAK, MRET, CSR ops) ──────────────────── */
    case OP_SYSTEM: {
        uint32_t csr_addr = insn >> 20;

        if (funct3 == F3_ECALL) {
            if (csr_addr == 0x000) {
                /* ECALL */
                trap(cpu, CAUSE_ECALL_M, 0);
                return;   /* don't advance PC — trap handler sets it */
            } else if (csr_addr == 0x001) {
                /* EBREAK */
                trap(cpu, CAUSE_BREAKPOINT, cpu->pc);
                return;
            } else if (csr_addr == 0x302) {
                /* MRET */
                next_pc = cpu->mepc;
                /* Restore MIE from MPIE */
                if (cpu->mstatus & MSTATUS_MPIE)
                    cpu->mstatus |= MSTATUS_MIE;
                else
                    cpu->mstatus &= ~MSTATUS_MIE;
                cpu->mstatus |= MSTATUS_MPIE;
                break;
            } else if (csr_addr == 0x105) {
                /* WFI — treat as NOP */
                break;
            } else {
                trap(cpu, CAUSE_ILLEGAL_INSN, insn);
                return;
            }
        }

        /* CSR instructions */
        uint32_t csr_val = csr_read(cpu, csr_addr);
        uint32_t write_val = 0;
        bool do_write = false;

        switch (funct3) {
        case F3_CSRRW:
            write_val = cpu->x[rs1];
            do_write  = true;
            if (rd) cpu->x[rd] = csr_val;
            break;
        case F3_CSRRS:
            if (rd) cpu->x[rd] = csr_val;
            if (rs1) { write_val = csr_val | cpu->x[rs1]; do_write = true; }
            break;
        case F3_CSRRC:
            if (rd) cpu->x[rd] = csr_val;
            if (rs1) { write_val = csr_val & ~cpu->x[rs1]; do_write = true; }
            break;
        case F3_CSRRWI:
            write_val = rs1;   /* rs1 field used as immediate */
            do_write  = true;
            if (rd) cpu->x[rd] = csr_val;
            break;
        case F3_CSRRSI:
            if (rd) cpu->x[rd] = csr_val;
            if (rs1) { write_val = csr_val | rs1; do_write = true; }
            break;
        case F3_CSRRCI:
            if (rd) cpu->x[rd] = csr_val;
            if (rs1) { write_val = csr_val & ~rs1; do_write = true; }
            break;
        default:
            trap(cpu, CAUSE_ILLEGAL_INSN, insn);
            return;
        }
        if (do_write) csr_write(cpu, csr_addr, write_val);
        break;
    }

    default:
        trap(cpu, CAUSE_ILLEGAL_INSN, insn);
        return;
    }

    cpu->x[0] = 0;   /* x0 hardwired to zero */
    cpu->pc   = next_pc;
    cpu->insn_count++;
}
