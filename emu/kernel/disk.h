#ifndef KERNEL_DISK_H
#define KERNEL_DISK_H

#include <stdint.h>

#define DISK_SECTOR_SIZE  512

void disk_driver_init(void);
int  disk_read_sector(uint32_t sector, void *buf);
int  disk_write_sector(uint32_t sector, const void *buf);

#endif
