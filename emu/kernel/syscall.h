#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "trap.h"

/* System call numbers (in a7) */
#define SYS_YIELD       0
#define SYS_EXIT        1
#define SYS_WRITE       2
#define SYS_READ        3
#define SYS_SLEEP_MS    4
#define SYS_GETTIME     5
#define SYS_MALLOC      6
#define SYS_FREE        7
#define SYS_OPEN        10
#define SYS_CLOSE       11
#define SYS_FREAD       12
#define SYS_FWRITE      13
#define SYS_FSTAT       14
#define SYS_READDIR     15
#define SYS_MKDIR       16
#define SYS_UNLINK      17
#define SYS_FB_FLUSH    20
#define SYS_INPUT_POLL  21
#define SYS_REALLOC     22

/* Called from trap.c on ecall */
void syscall_dispatch(trap_frame_t *frame);

#endif
