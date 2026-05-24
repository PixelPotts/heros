#include "syscall.h"
#include "sched.h"
#include "timer.h"
#include "mm.h"
#include "uart.h"
#include "kprintf.h"
#include "string.h"

/* Forward declarations for drivers that will be added in Phase 2 */
extern void fb_driver_flush(void);
extern int  input_driver_poll(void *event_out);
extern int  fs_open(const char *path, int flags);
extern int  fs_close(int fd);
extern int  fs_read(int fd, void *buf, int size);
extern int  fs_write(int fd, const void *buf, int size);
extern int  fs_stat(const char *path, void *stat_out);
extern int  fs_readdir(int fd, void *entry_out);
extern int  fs_mkdir(const char *path);
extern int  fs_unlink(const char *path);

void syscall_dispatch(trap_frame_t *frame)
{
    uint32_t num = frame->a7;

    switch (num) {
    case SYS_YIELD:
        /* handled by returning and letting scheduler run */
        break;

    case SYS_EXIT:
        sched_exit();
        break;

    case SYS_WRITE: {
        /* a0 = fd (0=stdout), a1 = buf, a2 = len */
        const char *buf = (const char *)(uintptr_t)frame->a1;
        uint32_t len = frame->a2;
        if (frame->a0 == 1 || frame->a0 == 0) {
            for (uint32_t i = 0; i < len; i++)
                uart_putchar(buf[i]);
        }
        frame->a0 = len;
        break;
    }

    case SYS_READ: {
        /* a0 = fd (0=stdin), a1 = buf, a2 = max_len */
        if (frame->a0 == 0) {
            int ch = uart_getchar();
            if (ch >= 0) {
                char *buf = (char *)(uintptr_t)frame->a1;
                buf[0] = (char)ch;
                frame->a0 = 1;
            } else {
                frame->a0 = 0;
            }
        } else {
            frame->a0 = (uint32_t)-1;
        }
        break;
    }

    case SYS_SLEEP_MS:
        sched_sleep_ms(frame->a0);
        break;

    case SYS_GETTIME:
        frame->a0 = timer_uptime_ms();
        break;

    case SYS_MALLOC:
        frame->a0 = (uint32_t)(uintptr_t)kmalloc(frame->a0);
        break;

    case SYS_FREE:
        kfree((void *)(uintptr_t)frame->a0);
        frame->a0 = 0;
        break;

    case SYS_REALLOC:
        frame->a0 = (uint32_t)(uintptr_t)krealloc(
            (void *)(uintptr_t)frame->a0, frame->a1, frame->a2);
        break;

    case SYS_OPEN:
        frame->a0 = (uint32_t)fs_open(
            (const char *)(uintptr_t)frame->a0, (int)frame->a1);
        break;

    case SYS_CLOSE:
        frame->a0 = (uint32_t)fs_close((int)frame->a0);
        break;

    case SYS_FREAD:
        frame->a0 = (uint32_t)fs_read(
            (int)frame->a0, (void *)(uintptr_t)frame->a1, (int)frame->a2);
        break;

    case SYS_FWRITE:
        frame->a0 = (uint32_t)fs_write(
            (int)frame->a0, (const void *)(uintptr_t)frame->a1, (int)frame->a2);
        break;

    case SYS_FSTAT:
        frame->a0 = (uint32_t)fs_stat(
            (const char *)(uintptr_t)frame->a0, (void *)(uintptr_t)frame->a1);
        break;

    case SYS_READDIR:
        frame->a0 = (uint32_t)fs_readdir(
            (int)frame->a0, (void *)(uintptr_t)frame->a1);
        break;

    case SYS_MKDIR:
        frame->a0 = (uint32_t)fs_mkdir(
            (const char *)(uintptr_t)frame->a0);
        break;

    case SYS_UNLINK:
        frame->a0 = (uint32_t)fs_unlink(
            (const char *)(uintptr_t)frame->a0);
        break;

    case SYS_FB_FLUSH:
        fb_driver_flush();
        frame->a0 = 0;
        break;

    case SYS_INPUT_POLL:
        frame->a0 = (uint32_t)input_driver_poll(
            (void *)(uintptr_t)frame->a0);
        break;

    default:
        kprintf("[syscall] Unknown syscall %u from task %d\n",
                num, sched_current_id());
        frame->a0 = (uint32_t)-1;
        break;
    }
}
