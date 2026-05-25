/*
 * inject_file — Write a host file into a FAT16 disk image
 *
 * Usage: inject_file <disk.img> <host_file> <fat_path>
 *   e.g.: inject_file disk.img /tmp/red.bmp /test-img/red.bmp
 *
 * Creates directories as needed.  FAT16 8.3 names only.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR_SIZE 512

/* ── Little-endian helpers ───────────────────────────────────── */
static uint16_t get16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t get32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* ── BPB (cached from boot sector) ──────────────────────────── */
static uint16_t bytes_per_sec;
static uint8_t  secs_per_clus;
static uint16_t reserved_secs;
static uint8_t  num_fats;
static uint16_t root_entries;
static uint16_t fat_size_secs;
static uint32_t total_secs;

static uint32_t fat1_off;      /* byte offset of FAT1 */
static uint32_t root_off;      /* byte offset of root directory */
static uint32_t root_secs;     /* sectors occupied by root dir */
static uint32_t data_off;      /* byte offset of data area (cluster 2) */
static uint32_t total_clusters;

static FILE *disk;

static void read_bpb(void)
{
    uint8_t boot[SECTOR_SIZE];
    fseek(disk, 0, SEEK_SET);
    fread(boot, 1, SECTOR_SIZE, disk);

    bytes_per_sec = get16(boot + 11);
    secs_per_clus = boot[13];
    reserved_secs = get16(boot + 14);
    num_fats      = boot[16];
    root_entries  = get16(boot + 17);
    total_secs    = get16(boot + 19);
    if (total_secs == 0) total_secs = get32(boot + 32);
    fat_size_secs = get16(boot + 22);

    fat1_off   = (uint32_t)reserved_secs * bytes_per_sec;
    root_secs  = ((uint32_t)root_entries * 32 + bytes_per_sec - 1) / bytes_per_sec;
    root_off   = fat1_off + (uint32_t)num_fats * fat_size_secs * bytes_per_sec;
    data_off   = root_off + root_secs * bytes_per_sec;
    total_clusters = (total_secs - reserved_secs - num_fats * fat_size_secs - root_secs) / secs_per_clus;
}

/* ── FAT operations ──────────────────────────────────────────── */
static uint16_t fat_read(uint16_t cluster)
{
    uint8_t buf[2];
    fseek(disk, fat1_off + cluster * 2, SEEK_SET);
    fread(buf, 1, 2, disk);
    return get16(buf);
}

static void fat_write(uint16_t cluster, uint16_t val)
{
    uint8_t buf[2];
    put16(buf, val);
    /* Write both FATs */
    for (int f = 0; f < num_fats; f++) {
        uint32_t off = fat1_off + (uint32_t)f * fat_size_secs * bytes_per_sec + cluster * 2;
        fseek(disk, off, SEEK_SET);
        fwrite(buf, 1, 2, disk);
    }
}

static uint16_t fat_alloc(void)
{
    for (uint16_t c = 2; c < 2 + total_clusters; c++) {
        if (fat_read(c) == 0) {
            fat_write(c, 0xFFFF);  /* end-of-chain */
            return c;
        }
    }
    fprintf(stderr, "ERROR: FAT full\n");
    exit(1);
}

static uint32_t cluster_offset(uint16_t cluster)
{
    return data_off + (uint32_t)(cluster - 2) * secs_per_clus * bytes_per_sec;
}

/* ── Directory operations ────────────────────────────────────── */

/* Convert filename to FAT 8.3 format */
static void to_83(const char *name, uint8_t out[11])
{
    memset(out, ' ', 11);
    const char *dot = strrchr(name, '.');
    int base_len = dot ? (int)(dot - name) : (int)strlen(name);
    if (base_len > 8) base_len = 8;
    for (int i = 0; i < base_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = c;
    }
    if (dot) {
        dot++;
        for (int i = 0; i < 3 && dot[i]; i++) {
            char c = dot[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + i] = c;
        }
    }
}

/*
 * Find or create a directory entry.
 * dir_off/dir_size: byte offset & size in bytes of the directory data.
 * For root dir: dir_off = root_off, dir_size = root_entries * 32
 * For subdir:   dir_off = cluster_offset(clus), dir_size = secs_per_clus * bytes_per_sec
 * is_root: 1 for root directory
 *
 * Returns the entry offset (byte offset in disk) or creates it.
 * Sets *out_cluster if it's a directory (returns the cluster of the found/created dir).
 */
static uint32_t find_or_create_entry(uint32_t dir_off, uint32_t dir_size,
                                      const char *name, int is_dir,
                                      uint16_t *out_cluster, int create_ok)
{
    uint8_t target[11];
    to_83(name, target);

    /* Search existing entries */
    for (uint32_t i = 0; i < dir_size; i += 32) {
        uint8_t ent[32];
        fseek(disk, dir_off + i, SEEK_SET);
        fread(ent, 1, 32, disk);

        if (ent[0] == 0x00) break;      /* end of directory */
        if (ent[0] == 0xE5) continue;    /* deleted */
        if (ent[11] == 0x0F) continue;   /* LFN */

        if (memcmp(ent, target, 11) == 0) {
            /* Found */
            *out_cluster = get16(ent + 26);
            return dir_off + i;
        }
    }

    if (!create_ok) return 0;

    /* Create new entry */
    for (uint32_t i = 0; i < dir_size; i += 32) {
        uint8_t ent[32];
        fseek(disk, dir_off + i, SEEK_SET);
        fread(ent, 1, 32, disk);

        if (ent[0] == 0x00 || ent[0] == 0xE5) {
            /* Free slot */
            memset(ent, 0, 32);
            memcpy(ent, target, 11);

            if (is_dir) {
                uint16_t clus = fat_alloc();
                ent[11] = 0x10;  /* directory attribute */
                put16(ent + 26, clus);
                put32(ent + 28, 0);

                /* Initialize the subdirectory cluster with . and .. */
                uint32_t coff = cluster_offset(clus);
                uint8_t zero[SECTOR_SIZE];
                memset(zero, 0, SECTOR_SIZE);
                for (int s = 0; s < secs_per_clus; s++) {
                    fseek(disk, coff + s * bytes_per_sec, SEEK_SET);
                    fwrite(zero, 1, bytes_per_sec, disk);
                }

                /* "." entry */
                uint8_t dot_ent[32];
                memset(dot_ent, 0, 32);
                memset(dot_ent, ' ', 11);
                dot_ent[0] = '.';
                dot_ent[11] = 0x10;
                put16(dot_ent + 26, clus);
                fseek(disk, coff, SEEK_SET);
                fwrite(dot_ent, 1, 32, disk);

                /* ".." entry */
                memset(dot_ent, ' ', 11);
                dot_ent[0] = '.';
                dot_ent[1] = '.';
                dot_ent[11] = 0x10;
                put16(dot_ent + 26, 0);  /* parent; 0 = root */
                fseek(disk, coff + 32, SEEK_SET);
                fwrite(dot_ent, 1, 32, disk);

                *out_cluster = clus;
            } else {
                ent[11] = 0x20;  /* archive attribute */
                *out_cluster = 0;
            }

            fseek(disk, dir_off + i, SEEK_SET);
            fwrite(ent, 1, 32, disk);
            return dir_off + i;
        }
    }

    fprintf(stderr, "ERROR: directory full\n");
    exit(1);
}

/* ── Navigate path, creating dirs as needed ──────────────────── */
/*
 * Walk the path like "/test-img/red.bmp", creating directories along the way.
 * Returns the byte offset of the final filename's entry slot.
 * Sets entry_cluster for the final entry.
 */
static uint32_t navigate_path(const char *fat_path, uint16_t *entry_cluster)
{
    /* Skip leading / */
    const char *p = fat_path;
    if (*p == '/') p++;

    uint32_t cur_dir_off  = root_off;
    uint32_t cur_dir_size = (uint32_t)root_entries * 32;
    int is_root = 1;

    while (*p) {
        /* Extract next component */
        const char *slash = strchr(p, '/');
        char comp[64];
        if (slash) {
            int len = (int)(slash - p);
            if (len > 63) len = 63;
            memcpy(comp, p, len);
            comp[len] = '\0';
            p = slash + 1;

            /* This is a directory component */
            uint16_t clus = 0;
            find_or_create_entry(cur_dir_off, cur_dir_size, comp, 1, &clus, 1);

            cur_dir_off  = cluster_offset(clus);
            cur_dir_size = (uint32_t)secs_per_clus * bytes_per_sec;
            is_root = 0;
        } else {
            /* This is the filename */
            uint32_t ent_off = find_or_create_entry(cur_dir_off, cur_dir_size,
                                                     p, 0, entry_cluster, 1);
            return ent_off;
        }
    }

    (void)is_root;
    fprintf(stderr, "ERROR: no filename in path\n");
    exit(1);
}

/* ── Main ────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk.img> <host_file> <fat_path>\n", argv[0]);
        fprintf(stderr, "  e.g.: %s disk.img /tmp/red.bmp /test-img/red.bmp\n", argv[0]);
        return 1;
    }

    const char *disk_path = argv[1];
    const char *host_file = argv[2];
    const char *fat_path  = argv[3];

    /* Read host file */
    FILE *hf = fopen(host_file, "rb");
    if (!hf) { perror("fopen host file"); return 1; }
    fseek(hf, 0, SEEK_END);
    long file_size = ftell(hf);
    fseek(hf, 0, SEEK_SET);
    uint8_t *file_data = malloc(file_size);
    if (!file_data) { fprintf(stderr, "malloc failed\n"); return 1; }
    fread(file_data, 1, file_size, hf);
    fclose(hf);

    /* Open disk image */
    disk = fopen(disk_path, "r+b");
    if (!disk) { perror("fopen disk"); free(file_data); return 1; }

    read_bpb();

    /* Navigate/create path */
    uint16_t entry_cluster = 0;
    uint32_t ent_off = navigate_path(fat_path, &entry_cluster);

    /* Allocate clusters for file data */
    uint32_t clus_size = (uint32_t)secs_per_clus * bytes_per_sec;
    uint32_t clusters_needed = (file_size + clus_size - 1) / clus_size;
    if (clusters_needed == 0) clusters_needed = 1;

    uint16_t first_clus = 0;
    uint16_t prev_clus = 0;

    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t c = fat_alloc();
        if (i == 0) first_clus = c;
        if (prev_clus != 0) fat_write(prev_clus, c);
        prev_clus = c;

        /* Write data to cluster */
        uint32_t off = cluster_offset(c);
        uint32_t chunk = file_size - i * clus_size;
        if (chunk > clus_size) chunk = clus_size;

        fseek(disk, off, SEEK_SET);
        fwrite(file_data + i * clus_size, 1, chunk, disk);

        /* Zero-fill remainder of last cluster */
        if (chunk < clus_size) {
            uint8_t zero[512];
            memset(zero, 0, 512);
            uint32_t rem = clus_size - chunk;
            while (rem > 0) {
                uint32_t w = rem > 512 ? 512 : rem;
                fwrite(zero, 1, w, disk);
                rem -= w;
            }
        }
    }

    /* Update directory entry with first cluster and size */
    uint8_t ent[32];
    fseek(disk, ent_off, SEEK_SET);
    fread(ent, 1, 32, disk);
    put16(ent + 26, first_clus);
    put32(ent + 28, (uint32_t)file_size);
    fseek(disk, ent_off, SEEK_SET);
    fwrite(ent, 1, 32, disk);

    fclose(disk);
    free(file_data);

    printf("Injected %s (%ld bytes) → %s\n", host_file, file_size, fat_path);
    return 0;
}
