#include "clint.h"

static uint64_t mtime;
static uint64_t mtimecmp;
static uint32_t msip;

void clint_init(void)
{
    mtime    = 0;
    mtimecmp = UINT64_MAX;   /* no interrupt until kernel sets it */
    msip     = 0;
}

uint32_t clint_read(uint32_t offset)
{
    switch (offset) {
    case CLINT_MSIP:          return msip;
    case CLINT_MTIMECMP_LO:   return (uint32_t)(mtimecmp);
    case CLINT_MTIMECMP_HI:   return (uint32_t)(mtimecmp >> 32);
    case CLINT_MTIME_LO:      return (uint32_t)(mtime);
    case CLINT_MTIME_HI:      return (uint32_t)(mtime >> 32);
    default:                   return 0;
    }
}

void clint_write(uint32_t offset, uint32_t val)
{
    switch (offset) {
    case CLINT_MSIP:
        msip = val & 1;
        break;
    case CLINT_MTIMECMP_LO:
        mtimecmp = (mtimecmp & 0xFFFFFFFF00000000ULL) | val;
        break;
    case CLINT_MTIMECMP_HI:
        mtimecmp = (mtimecmp & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32);
        break;
    case CLINT_MTIME_LO:
        mtime = (mtime & 0xFFFFFFFF00000000ULL) | val;
        break;
    case CLINT_MTIME_HI:
        mtime = (mtime & 0x00000000FFFFFFFFULL) | ((uint64_t)val << 32);
        break;
    }
}

void clint_tick(uint32_t ticks)
{
    mtime += ticks;
}

bool clint_timer_pending(void)
{
    return mtime >= mtimecmp;
}

bool clint_software_pending(void)
{
    return (msip & 1) != 0;
}
