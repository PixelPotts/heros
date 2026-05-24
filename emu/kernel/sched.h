#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include "trap.h"
#include <stdint.h>

#define MAX_TASKS       16
#define TASK_STACK_SIZE  (8 * 1024)   /* 8 KB per task stack */

typedef enum {
    TASK_FREE = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct {
    uint32_t      sp;            /* saved stack pointer */
    uint32_t      stack_base;    /* bottom of stack allocation */
    task_state_t  state;
    uint32_t      wake_time;     /* for sleep_ms */
    char          name[32];
    int           id;
} task_t;

void    sched_init(void);
int     sched_create_task(const char *name, void (*entry)(void));
void    sched_tick(trap_frame_t *frame);  /* called from timer interrupt */
void    sched_yield(void);
void    sched_exit(void);
void    sched_sleep_ms(uint32_t ms);
void    sched_block(void);
void    sched_unblock(int task_id);

int     sched_current_id(void);
const char *sched_task_name(int id);
task_state_t sched_task_state(int id);
int     sched_task_count(void);

/* Enable scheduling (called after first task created) */
void    sched_start(void);

#endif
