#include "disk.h"
#include "string.h"
#include "kprintf.h"

/* Disk MMIO registers */
#define DISK_BASE       0x10001000
#define DISK_REG_SECTOR (*(volatile uint32_t *)(DISK_BASE + 0x00))
#define DISK_REG_BUFFER (*(volatile uint32_t *)(DISK_BASE + 0x08))
#define DISK_REG_CMD    (*(volatile uint32_t *)(DISK_BASE + 0x10))
#define DISK_REG_STATUS (*(volatile uint32_t *)(DISK_BASE + 0x18))

/* DMA buffer in RAM */
static uint8_t __attribute__((aligned(4))) dma_buffer[DISK_SECTOR_SIZE];

void disk_driver_init(void)
{
    DISK_REG_STATUS = 0;  /* clear any pending status */
    kprintf("[disk] Block device initialized\n");
}

int disk_read_sector(uint32_t sector, void *buf)
{
    DISK_REG_SECTOR = sector;
    DISK_REG_BUFFER = (uint32_t)(uintptr_t)dma_buffer;
    DISK_REG_CMD = 1;  /* read */

    /* Poll for completion */
    while (DISK_REG_STATUS == 1)
        ;

    if (DISK_REG_STATUS == 2) {
        memcpy(buf, dma_buffer, DISK_SECTOR_SIZE);
        DISK_REG_STATUS = 0;
        return 0;
    }

    kprintf("[disk] Read error, sector %u, status %u\n",
            sector, DISK_REG_STATUS);
    DISK_REG_STATUS = 0;
    return -1;
}

int disk_write_sector(uint32_t sector, const void *buf)
{
    memcpy(dma_buffer, buf, DISK_SECTOR_SIZE);
    DISK_REG_SECTOR = sector;
    DISK_REG_BUFFER = (uint32_t)(uintptr_t)dma_buffer;
    DISK_REG_CMD = 2;  /* write */

    while (DISK_REG_STATUS == 1)
        ;

    if (DISK_REG_STATUS == 2) {
        DISK_REG_STATUS = 0;
        return 0;
    }

    kprintf("[disk] Write error, sector %u, status %u\n",
            sector, DISK_REG_STATUS);
    DISK_REG_STATUS = 0;
    return -1;
}
