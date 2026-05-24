#include "sched.h"
#include "mm.h"
#include "timer.h"
#include "string.h"
#include "kprintf.h"

static task_t    tasks[MAX_TASKS];
static int       current_task = -1;
static int       task_count = 0;
static int       sched_enabled = 0;

/* Saved trap frame pointer for the current task during context switch */
static trap_frame_t *current_frame __attribute__((unused)) = (void *)0;

void sched_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    current_task = -1;
    task_count = 0;
    sched_enabled = 0;

    /* Task 0 is the kernel/idle task (already running) */
    tasks[0].state = TASK_RUNNING;
    tasks[0].id = 0;
    tasks[0].sp = 0;  /* uses boot stack */
    tasks[0].stack_base = 0;
    strncpy(tasks[0].name, "idle", sizeof(tasks[0].name));
    current_task = 0;
    task_count = 1;

    kprintf("[sched] Initialized, idle task = 0\n");
}

/* Task wrapper — calls entry, then exits */
static void task_wrapper(void)
{
    /* The entry function pointer is stored in s0 of the initial frame */
    void (*entry)(void);
    __asm__ volatile("mv %0, s0" : "=r"(entry));
    entry();
    sched_exit();
}

int sched_create_task(const char *name, void (*entry)(void))
{
    /* Find free slot */
    int id = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE || tasks[i].state == TASK_DEAD) {
            id = i;
            break;
        }
    }
    if (id < 0) {
        kprintf("[sched] No free task slots\n");
        return -1;
    }

    /* Allocate stack */
    void *stack = page_alloc(TASK_STACK_SIZE / PAGE_SIZE);
    if (!stack) {
        kprintf("[sched] Cannot allocate stack for task '%s'\n", name);
        return -1;
    }

    uint32_t stack_top = (uint32_t)(uintptr_t)stack + TASK_STACK_SIZE;

    /* Build initial trap frame on the stack */
    stack_top -= sizeof(trap_frame_t);
    trap_frame_t *frame = (trap_frame_t *)stack_top;
    memset(frame, 0, sizeof(trap_frame_t));

    frame->mepc = (uint32_t)(uintptr_t)task_wrapper;
    frame->sp = stack_top;
    frame->s0 = (uint32_t)(uintptr_t)entry;  /* pass entry via s0 */
    frame->ra = (uint32_t)(uintptr_t)sched_exit;

    tasks[id].sp = stack_top;
    tasks[id].stack_base = (uint32_t)(uintptr_t)stack;
    tasks[id].state = TASK_READY;
    tasks[id].id = id;
    tasks[id].wake_time = 0;
    strncpy(tasks[id].name, name, sizeof(tasks[id].name) - 1);
    tasks[id].name[sizeof(tasks[id].name) - 1] = '\0';

    task_count++;
    kprintf("[sched] Created task %d: '%s'\n", id, name);
    return id;
}

static int find_next_task(void)
{
    /* Wake up sleeping tasks */
    uint32_t now = timer_uptime_ms();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_time) {
            tasks[i].state = TASK_READY;
        }
    }

    /* Round-robin from current+1 */
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_task + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY)
            return idx;
    }
    return 0;  /* idle task */
}

void sched_tick(trap_frame_t *frame)
{
    if (!sched_enabled) return;

    /* Save current task's frame */
    if (current_task >= 0 && tasks[current_task].state == TASK_RUNNING) {
        tasks[current_task].sp = (uint32_t)(uintptr_t)frame;
        tasks[current_task].state = TASK_READY;
    }

    /* Find next task */
    int next = find_next_task();

    if (next != current_task) {
        current_task = next;
        tasks[current_task].state = TASK_RUNNING;

        /* Switch to next task's trap frame by updating the frame in place */
        trap_frame_t *next_frame = (trap_frame_t *)(uintptr_t)tasks[current_task].sp;

        /* Copy next task's saved frame into the trap frame location */
        memcpy(frame, next_frame, sizeof(trap_frame_t));

        /* Update mscratch to point to new task's stack */
        uint32_t new_scratch = tasks[current_task].sp;
        __asm__ volatile("csrw mscratch, %0" :: "r"(new_scratch));
    } else {
        tasks[current_task].state = TASK_RUNNING;
    }
}

void sched_yield(void)
{
    /* Trigger ecall — trap handler will call sched_tick */
    __asm__ volatile("ecall");
}

void sched_exit(void)
{
    if (current_task > 0) {
        tasks[current_task].state = TASK_DEAD;
        task_count--;
        kprintf("[sched] Task %d ('%s') exited\n",
                current_task, tasks[current_task].name);
    }
    /* Yield to let scheduler pick next task */
    while (1) sched_yield();
}

void sched_sleep_ms(uint32_t ms)
{
    if (current_task <= 0) return;  /* don't sleep idle task */
    tasks[current_task].state = TASK_SLEEPING;
    tasks[current_task].wake_time = timer_uptime_ms() + ms;
    sched_yield();
}

void sched_block(void)
{
    if (current_task <= 0) return;
    tasks[current_task].state = TASK_BLOCKED;
    sched_yield();
}

void sched_unblock(int task_id)
{
    if (task_id >= 0 && task_id < MAX_TASKS &&
        tasks[task_id].state == TASK_BLOCKED) {
        tasks[task_id].state = TASK_READY;
    }
}

void sched_start(void)
{
    sched_enabled = 1;
    /* Enable machine timer interrupt (MIE bit 7) */
    __asm__ volatile("csrs mie, %0" :: "r"(1 << 7));
    /* Enable global machine interrupts (MSTATUS.MIE bit 3) */
    __asm__ volatile("csrs mstatus, %0" :: "r"(1 << 3));
    kprintf("[sched] Scheduler started\n");
}

int sched_current_id(void)
{
    return current_task;
}

const char *sched_task_name(int id)
{
    if (id < 0 || id >= MAX_TASKS) return "???";
    return tasks[id].name;
}

task_state_t sched_task_state(int id)
{
    if (id < 0 || id >= MAX_TASKS) return TASK_FREE;
    return tasks[id].state;
}

int sched_task_count(void)
{
    return task_count;
}
