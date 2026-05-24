/*
 * mkfat16 — Create a FAT16 disk image for HerOS
 *
 * Usage: mkfat16 <output.img> [size_in_MB]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR_SIZE     512
#define DEFAULT_SIZE_MB 8

/* Write a 16-bit value in little-endian */
static void put16(uint8_t *buf, uint16_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

/* Write a 32-bit value in little-endian */
static void put32(uint8_t *buf, uint32_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static void write_fat_name(uint8_t *entry, const char *name, const char *ext)
{
    memset(entry, ' ', 11);
    for (int i = 0; i < 8 && name[i]; i++)
        entry[i] = name[i];
    for (int i = 0; i < 3 && ext[i]; i++)
        entry[8 + i] = ext[i];
}

static void add_dir_entry(uint8_t *dir_sector, int *entry_idx,
                           const char *name, const char *ext,
                           uint8_t attr, uint16_t cluster, uint32_t size)
{
    uint8_t *e = dir_sector + (*entry_idx) * 32;
    write_fat_name(e, name, ext);
    e[11] = attr;
    memset(e + 12, 0, 10);
    put16(e + 26, cluster);
    put32(e + 28, size);
    (*entry_idx)++;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <output.img> [size_MB]\n", argv[0]);
        return 1;
    }

    const char *output = argv[1];
    int size_mb = (argc >= 3) ? atoi(argv[2]) : DEFAULT_SIZE_MB;
    if (size_mb < 1) size_mb = 1;
    if (size_mb > 128) size_mb = 128;

    uint32_t total_sectors = (uint32_t)size_mb * 1024 * 1024 / SECTOR_SIZE;

    /* FAT16 parameters */
    uint8_t  sectors_per_cluster = 4;   /* 2 KB clusters */
    uint16_t reserved_sectors = 1;
    uint8_t  num_fats = 2;
    uint16_t root_entries = 512;
    uint16_t fat_size;

    /* Calculate FAT size */
    uint32_t root_sectors = (root_entries * 32 + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint32_t data_sectors = total_sectors - reserved_sectors - root_sectors;
    /* Approximate: data_sectors / sectors_per_cluster needs FAT entries */
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    fat_size = (uint16_t)((total_clusters * 2 + SECTOR_SIZE - 1) / SECTOR_SIZE);
    /* Recalculate with FAT subtracted */
    data_sectors = total_sectors - reserved_sectors - (num_fats * fat_size) - root_sectors;
    total_clusters = data_sectors / sectors_per_cluster;

    printf("Creating FAT16 image: %s\n", output);
    printf("  Size: %d MB (%u sectors)\n", size_mb, total_sectors);
    printf("  Cluster size: %d sectors (%d bytes)\n",
           sectors_per_cluster, sectors_per_cluster * SECTOR_SIZE);
    printf("  FAT size: %u sectors, %u data clusters\n", fat_size, total_clusters);

    FILE *fp = fopen(output, "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    /* Write empty image */
    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, SECTOR_SIZE);
    for (uint32_t i = 0; i < total_sectors; i++)
        fwrite(zero, 1, SECTOR_SIZE, fp);

    /* ── Write boot sector (BPB) ────────────────────────────── */
    uint8_t boot[SECTOR_SIZE];
    memset(boot, 0, SECTOR_SIZE);

    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;  /* JMP short */
    memcpy(boot + 3, "HEROSI  ", 8);                    /* OEM name */
    put16(boot + 11, SECTOR_SIZE);                       /* bytes per sector */
    boot[13] = sectors_per_cluster;
    put16(boot + 14, reserved_sectors);
    boot[16] = num_fats;
    put16(boot + 17, root_entries);
    if (total_sectors < 65536)
        put16(boot + 19, (uint16_t)total_sectors);
    else
        put32(boot + 32, total_sectors);
    boot[21] = 0xF8;                                     /* media descriptor */
    put16(boot + 22, fat_size);
    put16(boot + 24, 63);                                /* sectors per track */
    put16(boot + 26, 255);                               /* heads */
    put32(boot + 28, 0);                                 /* hidden sectors */

    /* Extended boot record */
    boot[36] = 0x80;                                     /* drive number */
    boot[38] = 0x29;                                     /* extended boot sig */
    put32(boot + 39, 0xDEADBEEF);                        /* volume serial */
    memcpy(boot + 43, "HEROS      ", 11);                /* volume label */
    memcpy(boot + 54, "FAT16   ", 8);                    /* filesystem type */

    boot[510] = 0x55;
    boot[511] = 0xAA;

    fseek(fp, 0, SEEK_SET);
    fwrite(boot, 1, SECTOR_SIZE, fp);

    /* ── Write FAT ──────────────────────────────────────────── */
    uint8_t fat_sector[SECTOR_SIZE];
    memset(fat_sector, 0, SECTOR_SIZE);

    /* First two FAT entries are reserved */
    fat_sector[0] = 0xF8;  /* media descriptor */
    fat_sector[1] = 0xFF;
    fat_sector[2] = 0xFF;  /* end-of-chain marker */
    fat_sector[3] = 0xFF;

    /* Cluster 2 = DOCS directory (mark end-of-chain) */
    fat_sector[4] = 0xFF;
    fat_sector[5] = 0xFF;

    /* Cluster 3 = readme.txt data (single cluster, end-of-chain) */
    fat_sector[6] = 0xFF;
    fat_sector[7] = 0xFF;

    /* Write FAT1 */
    uint32_t fat1_start = reserved_sectors;
    fseek(fp, fat1_start * SECTOR_SIZE, SEEK_SET);
    fwrite(fat_sector, 1, SECTOR_SIZE, fp);

    /* Write FAT2 */
    uint32_t fat2_start = fat1_start + fat_size;
    fseek(fp, fat2_start * SECTOR_SIZE, SEEK_SET);
    fwrite(fat_sector, 1, SECTOR_SIZE, fp);

    /* ── Write root directory ───────────────────────────────── */
    uint32_t root_start = fat2_start + fat_size;
    uint8_t root_sec[SECTOR_SIZE];
    memset(root_sec, 0, SECTOR_SIZE);

    int eidx = 0;

    /* Volume label */
    add_dir_entry(root_sec, &eidx, "HEROS", "   ", 0x08, 0, 0);

    /* DOCS directory */
    add_dir_entry(root_sec, &eidx, "DOCS", "   ", 0x10, 2, 0);

    /* readme.txt */
    const char *readme_text = "Welcome to HerOS!\nThis is a bare-metal operating system for RISC-V.\n";
    uint32_t readme_len = (uint32_t)strlen(readme_text);
    add_dir_entry(root_sec, &eidx, "README", "TXT", 0x20, 3, readme_len);

    fseek(fp, root_start * SECTOR_SIZE, SEEK_SET);
    fwrite(root_sec, 1, SECTOR_SIZE, fp);

    /* ── Write DOCS directory (cluster 2) ───────────────────── */
    uint32_t data_start = root_start + root_sectors;
    uint32_t docs_sector = data_start; /* cluster 2 = first data sector */
    uint8_t docs_dir[SECTOR_SIZE];
    memset(docs_dir, 0, SECTOR_SIZE);

    int didx = 0;
    add_dir_entry(docs_dir, &didx, ".", "   ", 0x10, 2, 0);
    add_dir_entry(docs_dir, &didx, "..", "   ", 0x10, 0, 0);

    fseek(fp, docs_sector * SECTOR_SIZE, SEEK_SET);
    fwrite(docs_dir, 1, SECTOR_SIZE, fp);

    /* ── Write readme.txt data (cluster 3) ──────────────────── */
    uint32_t readme_sector = data_start + sectors_per_cluster; /* cluster 3 */
    uint8_t readme_data[SECTOR_SIZE];
    memset(readme_data, 0, SECTOR_SIZE);
    memcpy(readme_data, readme_text, readme_len);

    fseek(fp, readme_sector * SECTOR_SIZE, SEEK_SET);
    fwrite(readme_data, 1, SECTOR_SIZE, fp);

    fclose(fp);
    printf("Done: %s\n", output);
    return 0;
}
