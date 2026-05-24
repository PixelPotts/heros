#include "disk.h"
#include <stdio.h>
#include <string.h>

static FILE    *disk_fp;
static uint8_t *ram;

/* MMIO registers */
static uint32_t reg_sector;
static uint32_t reg_buffer;   /* RAM address for DMA */
static uint32_t reg_cmd;
static uint32_t reg_status;   /* 0=idle, 1=busy, 2=done, 3=error */

void disk_init(const char *image_path, uint8_t *ram_ptr)
{
    ram = ram_ptr;
    reg_status = 0;
    if (image_path)
        disk_fp = fopen(image_path, "r+b");
    else
        disk_fp = NULL;
}

static void disk_exec_cmd(void)
{
    if (!disk_fp) { reg_status = 3; return; }

    /* Compute RAM offset — buffer addr is an absolute address */
    if (reg_buffer < RAM_BASE || reg_buffer + DISK_SECTOR_SIZE > RAM_BASE + RAM_SIZE) {
        reg_status = 3;
        return;
    }
    uint32_t ram_off = reg_buffer - RAM_BASE;

    off_t file_off = (off_t)reg_sector * DISK_SECTOR_SIZE;
    fseek(disk_fp, file_off, SEEK_SET);

    if (reg_cmd == 1) {
        /* Read sector → RAM */
        if (fread(ram + ram_off, 1, DISK_SECTOR_SIZE, disk_fp) == DISK_SECTOR_SIZE)
            reg_status = 2;   /* done */
        else
            reg_status = 3;   /* error */
    } else if (reg_cmd == 2) {
        /* Write RAM → sector */
        if (fwrite(ram + ram_off, 1, DISK_SECTOR_SIZE, disk_fp) == DISK_SECTOR_SIZE) {
            fflush(disk_fp);
            reg_status = 2;
        } else {
            reg_status = 3;
        }
    } else {
        reg_status = 3;
    }
}

uint32_t disk_read(uint32_t offset)
{
    switch (offset) {
    case DISK_SECTOR:   return reg_sector;
    case DISK_BUFFER:   return reg_buffer;
    case DISK_CMD:      return reg_cmd;
    case DISK_STATUS:   return reg_status;
    default:            return 0;
    }
}

void disk_write(uint32_t offset, uint32_t val)
{
    switch (offset) {
    case DISK_SECTOR:   reg_sector = val;  break;
    case DISK_BUFFER:   reg_buffer = val;  break;
    case DISK_CMD:
        reg_cmd = val;
        reg_status = 1;   /* busy */
        disk_exec_cmd();  /* instant DMA */
        break;
    case DISK_STATUS:
        reg_status = 0;   /* write to clear */
        break;
    }
}

void disk_close(void)
{
    if (disk_fp) {
        fclose(disk_fp);
        disk_fp = NULL;
    }
}
