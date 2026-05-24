#ifndef EMU_H
#define EMU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Memory Map ─────────────────────────────────────────────────── */
#define BOOT_ROM_BASE   0x00000000
#define BOOT_ROM_SIZE   0x00001000          /* 4 KB */

#define CLINT_BASE      0x02000000
#define CLINT_SIZE      0x00010000          /* 64 KB */

#define UART_BASE       0x10000000
#define UART_SIZE       0x00000008

#define DISK_BASE       0x10001000
#define DISK_SIZE       0x00000020

#define FB_BASE         0x20000000
#define FB_WIDTH        2560
#define FB_HEIGHT       1440
#define FB_BPP          4                   /* RGBA */
#define FB_SIZE         (FB_WIDTH * FB_HEIGHT * FB_BPP)  /* 14,745,600 */

#define FB_CTRL_BASE    0x21000000          /* after 16 MB framebuffer region */
#define FB_CTRL_SIZE    0x00000010

#define RAM_BASE        0x80000000
#define RAM_SIZE        0x08000000          /* 128 MB */

/* ── CLINT offsets ──────────────────────────────────────────────── */
#define CLINT_MSIP          0x0000
#define CLINT_MTIMECMP_LO   0x4000
#define CLINT_MTIMECMP_HI   0x4004
#define CLINT_MTIME_LO      0xBFF8
#define CLINT_MTIME_HI      0xBFFC

/* ── Disk register offsets ──────────────────────────────────────── */
#define DISK_SECTOR     0x00
#define DISK_BUFFER     0x08
#define DISK_CMD        0x10
#define DISK_STATUS     0x18

#define DISK_SECTOR_SIZE 512

/* ── Framebuffer control offsets ────────────────────────────────── */
#define FB_CTRL_WIDTH   0x00
#define FB_CTRL_HEIGHT  0x04
#define FB_CTRL_FLUSH   0x08

/* ── CSR addresses ──────────────────────────────────────────────── */
#define CSR_MSTATUS     0x300
#define CSR_MIE         0x304
#define CSR_MTVEC       0x305
#define CSR_MSCRATCH    0x340
#define CSR_MEPC        0x341
#define CSR_MCAUSE      0x342
#define CSR_MTVAL       0x343
#define CSR_MIP         0x344
#define CSR_MHARTID     0xF14

/* ── mstatus bits ───────────────────────────────────────────────── */
#define MSTATUS_MIE     (1 << 3)
#define MSTATUS_MPIE    (1 << 7)

/* ── mie / mip bits ─────────────────────────────────────────────── */
#define MIP_MSIP        (1 << 3)
#define MIP_MTIP        (1 << 7)

/* ── Exception / interrupt causes ───────────────────────────────── */
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

#endif /* EMU_H */
