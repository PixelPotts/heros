#ifndef DISK_H
#define DISK_H

#include "emu.h"

/* ram_ptr is passed so DMA can access main memory */
void     disk_init(const char *image_path, uint8_t *ram_ptr);
uint32_t disk_read(uint32_t offset);
void     disk_write(uint32_t offset, uint32_t val);
void     disk_close(void);

#endif
