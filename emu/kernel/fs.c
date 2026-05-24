#include "fs.h"
#include "disk.h"
#include "string.h"
#include "kprintf.h"

/* ═══════════════════════════════════════════════════════════════════
   FAT16 filesystem implementation
   ═══════════════════════════════════════════════════════════════════ */

/* BPB (BIOS Parameter Block) fields */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} fat16_bpb_t;

/* Directory entry (32 bytes) */
typedef struct __attribute__((packed)) {
    char     name[11];      /* 8.3 format */
    uint8_t  attr;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster;
    uint32_t file_size;
} fat16_dirent_t;

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LFN        0x0F

/* Filesystem state */
static struct {
    int      mounted;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t fat_size;
    uint32_t total_sectors;

    uint32_t fat_start;         /* sector of first FAT */
    uint32_t root_start;        /* sector of root directory */
    uint32_t root_sectors;      /* sectors used by root dir */
    uint32_t data_start;        /* first data sector */
} fs;

/* Open file descriptors */
typedef struct {
    int      used;
    uint16_t start_cluster;     /* first cluster */
    uint16_t cur_cluster;       /* current cluster */
    uint32_t file_size;
    uint32_t position;          /* read/write position */
    uint8_t  is_dir;
    uint32_t dir_sector;        /* for readdir: current sector in dir */
    int      dir_entry;         /* for readdir: entry index within sector */
    int      dir_total_entries; /* total dir entries to iterate */
    char     path[FS_MAX_PATH];
} fd_entry_t;

static fd_entry_t fd_table[FS_MAX_OPEN];

/* Sector buffer */
static uint8_t sector_buf[512];

static uint32_t cluster_to_sector(uint16_t cluster)
{
    return fs.data_start + (cluster - 2) * fs.sectors_per_cluster;
}

static uint16_t fat_read(uint16_t cluster)
{
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs.fat_start + (fat_offset / 512);
    uint32_t offset = fat_offset % 512;

    if (disk_read_sector(fat_sector, sector_buf) != 0)
        return 0xFFFF;

    return (uint16_t)(sector_buf[offset] | (sector_buf[offset + 1] << 8));
}

static int fat_write(uint16_t cluster, uint16_t value)
{
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs.fat_start + (fat_offset / 512);
    uint32_t offset = fat_offset % 512;

    if (disk_read_sector(fat_sector, sector_buf) != 0)
        return -1;

    sector_buf[offset]     = (uint8_t)(value & 0xFF);
    sector_buf[offset + 1] = (uint8_t)(value >> 8);

    /* Write to both FAT copies */
    if (disk_write_sector(fat_sector, sector_buf) != 0)
        return -1;
    disk_write_sector(fat_sector + fs.fat_size, sector_buf);
    return 0;
}

/* Allocate a free cluster, mark it as end-of-chain */
static uint16_t fat_alloc(void)
{
    /* Scan FAT for a free entry (value == 0x0000) */
    uint32_t total_clusters = (fs.total_sectors - fs.data_start) / fs.sectors_per_cluster + 2;
    for (uint16_t c = 2; c < total_clusters && c < 0xFFF0; c++) {
        if (fat_read(c) == 0x0000) {
            fat_write(c, 0xFFFF);  /* mark as end-of-chain */
            return c;
        }
    }
    return 0;  /* no free cluster */
}

/* Free entire cluster chain starting at cluster */
static void fat_free_chain(uint16_t cluster)
{
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint16_t next = fat_read(cluster);
        fat_write(cluster, 0x0000);
        cluster = next;
    }
}

/* Convert 8.3 FAT name to readable name */
static void fat_name_to_string(const char *fat_name, char *out)
{
    int i;
    /* Copy base name, strip trailing spaces */
    for (i = 7; i >= 0 && fat_name[i] == ' '; i--)
        ;
    int base_len = i + 1;
    for (int j = 0; j < base_len; j++) {
        char c = fat_name[j];
        /* Convert to lowercase */
        if (c >= 'A' && c <= 'Z') c += 32;
        out[j] = c;
    }

    /* Check extension */
    if (fat_name[8] != ' ') {
        out[base_len++] = '.';
        for (i = 0; i < 3 && fat_name[8 + i] != ' '; i++) {
            char c = fat_name[8 + i];
            if (c >= 'A' && c <= 'Z') c += 32;
            out[base_len++] = c;
        }
    }
    out[base_len] = '\0';
}

/* Convert string to 8.3 FAT name */
static void string_to_fat_name(const char *name, char *fat_name)
{
    memset(fat_name, ' ', 11);

    const char *dot = strchr(name, '.');
    int base_len = dot ? (int)(dot - name) : (int)strlen(name);
    if (base_len > 8) base_len = 8;

    for (int i = 0; i < base_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[i] = c;
    }

    if (dot) {
        dot++;
        for (int i = 0; i < 3 && dot[i]; i++) {
            char c = dot[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            fat_name[8 + i] = c;
        }
    }
}

/* Find a directory entry in the root directory */
static int find_root_entry(const char *name, fat16_dirent_t *out)
{
    char fat_name[11];
    string_to_fat_name(name, fat_name);

    for (uint32_t s = 0; s < fs.root_sectors; s++) {
        if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
            return -1;

        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        int per_sector = 512 / sizeof(fat16_dirent_t);

        for (int i = 0; i < per_sector; i++) {
            if (entries[i].name[0] == 0x00) return -1;  /* end of dir */
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;  /* deleted */
            if (entries[i].attr & ATTR_VOLUME_ID) continue;
            if (entries[i].attr == ATTR_LFN) continue;

            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                memcpy(out, &entries[i], sizeof(fat16_dirent_t));
                return 0;
            }
        }
    }
    return -1;
}

/* Find entry in root dir and return its sector + index for modification */
static int find_root_entry_loc(const char *name, uint32_t *out_sector, int *out_idx)
{
    char fat_name[11];
    string_to_fat_name(name, fat_name);

    for (uint32_t s = 0; s < fs.root_sectors; s++) {
        if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
            return -1;

        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        int per_sector = 512 / sizeof(fat16_dirent_t);

        for (int i = 0; i < per_sector; i++) {
            if (entries[i].name[0] == 0x00) return -1;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr & ATTR_VOLUME_ID) continue;
            if (entries[i].attr == ATTR_LFN) continue;

            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                *out_sector = fs.root_start + s;
                *out_idx = i;
                return 0;
            }
        }
    }
    return -1;
}

/* Navigate path — returns cluster of parent dir and final name component */
static int resolve_path(const char *path, uint16_t *parent_cluster,
                         char *final_name)
{
    /* Simple: only root directory for now */
    /* Skip leading / */
    while (*path == '/') path++;
    if (*path == '\0') {
        *parent_cluster = 0;  /* root */
        final_name[0] = '\0';
        return 0;
    }

    /* For now, only single-level paths */
    strncpy(final_name, path, FS_MAX_NAME - 1);
    final_name[FS_MAX_NAME - 1] = '\0';
    *parent_cluster = 0;
    return 0;
}

void fs_init(void)
{
    memset(&fs, 0, sizeof(fs));
    memset(fd_table, 0, sizeof(fd_table));

    /* Read boot sector */
    if (disk_read_sector(0, sector_buf) != 0) {
        kprintf("[fs] Cannot read boot sector\n");
        return;
    }

    fat16_bpb_t *bpb = (fat16_bpb_t *)sector_buf;

    /* Validate */
    if (bpb->bytes_per_sector != 512) {
        kprintf("[fs] Unsupported sector size: %u\n", bpb->bytes_per_sector);
        return;
    }

    fs.bytes_per_sector = bpb->bytes_per_sector;
    fs.sectors_per_cluster = bpb->sectors_per_cluster;
    fs.reserved_sectors = bpb->reserved_sectors;
    fs.num_fats = bpb->num_fats;
    fs.root_entry_count = bpb->root_entry_count;
    fs.fat_size = bpb->fat_size_16;
    fs.total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;

    fs.fat_start = fs.reserved_sectors;
    fs.root_start = fs.fat_start + fs.num_fats * fs.fat_size;
    fs.root_sectors = (fs.root_entry_count * 32 + 511) / 512;
    fs.data_start = fs.root_start + fs.root_sectors;

    fs.mounted = 1;
    kprintf("[fs] FAT16 mounted: %u sectors, %u bytes/cluster, root at sector %u\n",
            (unsigned)fs.total_sectors,
            (unsigned)(fs.sectors_per_cluster * 512),
            (unsigned)fs.root_start);
}

int fs_open(const char *path, int flags)
{
    if (!fs.mounted) return -1;

    /* Find free fd */
    int fd = -1;
    for (int i = 0; i < FS_MAX_OPEN; i++) {
        if (!fd_table[i].used) { fd = i; break; }
    }
    if (fd < 0) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    resolve_path(path, &parent, name);

    if (name[0] == '\0') {
        /* Opening root directory */
        fd_table[fd].used = 1;
        fd_table[fd].start_cluster = 0;
        fd_table[fd].cur_cluster = 0;
        fd_table[fd].file_size = 0;
        fd_table[fd].position = 0;
        fd_table[fd].is_dir = 1;
        fd_table[fd].dir_sector = fs.root_start;
        fd_table[fd].dir_entry = 0;
        fd_table[fd].dir_total_entries = fs.root_entry_count;
        strncpy(fd_table[fd].path, path, FS_MAX_PATH - 1);
        return fd;
    }

    fat16_dirent_t entry;
    if (find_root_entry(name, &entry) == 0) {
        fd_table[fd].used = 1;
        fd_table[fd].start_cluster = entry.first_cluster;
        fd_table[fd].cur_cluster = entry.first_cluster;
        fd_table[fd].file_size = entry.file_size;
        fd_table[fd].position = 0;
        fd_table[fd].is_dir = (entry.attr & ATTR_DIRECTORY) ? 1 : 0;
        if (fd_table[fd].is_dir) {
            fd_table[fd].dir_sector = cluster_to_sector(entry.first_cluster);
            fd_table[fd].dir_entry = 0;
            fd_table[fd].dir_total_entries = fs.sectors_per_cluster * 16;
        }
        strncpy(fd_table[fd].path, path, FS_MAX_PATH - 1);
        return fd;
    }

    /* File not found — create if requested */
    if (flags & FS_O_CREATE) {
        /* Find empty dir entry in root */
        for (uint32_t s = 0; s < fs.root_sectors; s++) {
            if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
                return -1;
            fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
            int per_sector = 512 / 32;
            for (int i = 0; i < per_sector; i++) {
                if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                    string_to_fat_name(name, entries[i].name);
                    entries[i].attr = ATTR_ARCHIVE;
                    entries[i].first_cluster = 0;
                    entries[i].file_size = 0;
                    memset(entries[i].reserved, 0, 10);
                    entries[i].time = 0;
                    entries[i].date = 0;
                    disk_write_sector(fs.root_start + s, sector_buf);

                    fd_table[fd].used = 1;
                    fd_table[fd].start_cluster = 0;
                    fd_table[fd].cur_cluster = 0;
                    fd_table[fd].file_size = 0;
                    fd_table[fd].position = 0;
                    fd_table[fd].is_dir = 0;
                    strncpy(fd_table[fd].path, path, FS_MAX_PATH - 1);
                    return fd;
                }
            }
        }
        return -1;  /* root dir full */
    }

    return -1;  /* not found */
}

int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN || !fd_table[fd].used)
        return -1;
    fd_table[fd].used = 0;
    return 0;
}

int fs_read(int fd, void *buf, int size)
{
    if (fd < 0 || fd >= FS_MAX_OPEN || !fd_table[fd].used)
        return -1;
    if (fd_table[fd].is_dir) return -1;

    fd_entry_t *f = &fd_table[fd];
    if (f->start_cluster == 0) return 0;  /* empty file */

    uint8_t *out = (uint8_t *)buf;
    int total_read = 0;

    while (size > 0 && f->position < f->file_size) {
        uint32_t cluster_size = (uint32_t)fs.sectors_per_cluster * 512;
        uint32_t pos_in_cluster = f->position % cluster_size;
        uint32_t sector_in_cluster = pos_in_cluster / 512;
        uint32_t pos_in_sector = pos_in_cluster % 512;

        uint32_t sector = cluster_to_sector(f->cur_cluster) + sector_in_cluster;
        if (disk_read_sector(sector, sector_buf) != 0)
            return total_read > 0 ? total_read : -1;

        int avail = 512 - (int)pos_in_sector;
        int remain_file = (int)(f->file_size - f->position);
        int to_copy = size;
        if (to_copy > avail) to_copy = avail;
        if (to_copy > remain_file) to_copy = remain_file;

        memcpy(out, sector_buf + pos_in_sector, to_copy);
        out += to_copy;
        f->position += to_copy;
        size -= to_copy;
        total_read += to_copy;

        /* Advance cluster if needed */
        if (f->position % cluster_size == 0 && f->position < f->file_size) {
            uint16_t next = fat_read(f->cur_cluster);
            if (next >= 0xFFF8) break;  /* end of chain */
            f->cur_cluster = next;
        }
    }

    return total_read;
}

int fs_write(int fd, const void *buf, int size)
{
    /* Simplified: writes not fully implemented yet */
    (void)fd; (void)buf; (void)size;
    return -1;
}

int fs_stat(const char *path, void *stat_out)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    resolve_path(path, &parent, name);

    fs_stat_t *st = (fs_stat_t *)stat_out;

    if (name[0] == '\0') {
        /* Root directory */
        strcpy(st->name, "/");
        st->size = 0;
        st->type = FS_TYPE_DIR;
        return 0;
    }

    fat16_dirent_t entry;
    if (find_root_entry(name, &entry) == 0) {
        fat_name_to_string(entry.name, st->name);
        st->size = entry.file_size;
        st->type = (entry.attr & ATTR_DIRECTORY) ? FS_TYPE_DIR : FS_TYPE_FILE;
        return 0;
    }

    return -1;
}

int fs_readdir(int fd, void *entry_out)
{
    if (fd < 0 || fd >= FS_MAX_OPEN || !fd_table[fd].used)
        return -1;
    if (!fd_table[fd].is_dir) return -1;

    fd_entry_t *f = &fd_table[fd];
    fs_dirent_t *out = (fs_dirent_t *)entry_out;

    while (f->dir_entry < f->dir_total_entries) {
        /* Which sector? */
        int entries_per_sector = 512 / 32;
        uint32_t sector = f->dir_sector + (f->dir_entry / entries_per_sector);
        int idx = f->dir_entry % entries_per_sector;

        if (disk_read_sector(sector, sector_buf) != 0)
            return -1;

        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        f->dir_entry++;

        if (entries[idx].name[0] == 0x00) return 0;  /* end of dir */
        if ((uint8_t)entries[idx].name[0] == 0xE5) continue;  /* deleted */
        if (entries[idx].attr & ATTR_VOLUME_ID) continue;
        if (entries[idx].attr == ATTR_LFN) continue;

        fat_name_to_string(entries[idx].name, out->name);
        out->size = entries[idx].file_size;
        out->type = (entries[idx].attr & ATTR_DIRECTORY) ? FS_TYPE_DIR : FS_TYPE_FILE;
        return 1;
    }

    return 0;  /* end of directory */
}

int fs_mkdir(const char *path)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    resolve_path(path, &parent, name);
    if (name[0] == '\0') return -1;  /* can't mkdir root */

    /* Check if already exists */
    fat16_dirent_t existing;
    if (find_root_entry(name, &existing) == 0)
        return -1;  /* already exists */

    /* Allocate a cluster for the directory data */
    uint16_t cluster = fat_alloc();
    if (cluster == 0) return -1;

    /* Find an empty entry in root directory */
    for (uint32_t s = 0; s < fs.root_sectors; s++) {
        if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
            return -1;
        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        int per_sector = 512 / 32;
        for (int i = 0; i < per_sector; i++) {
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                string_to_fat_name(name, entries[i].name);
                entries[i].attr = ATTR_DIRECTORY;
                entries[i].first_cluster = cluster;
                entries[i].file_size = 0;
                memset(entries[i].reserved, 0, 10);
                entries[i].time = 0;
                entries[i].date = 0;
                disk_write_sector(fs.root_start + s, sector_buf);

                /* Initialize the directory cluster with "." and ".." */
                memset(sector_buf, 0, 512);
                fat16_dirent_t *dir = (fat16_dirent_t *)sector_buf;

                /* "." entry */
                memset(dir[0].name, ' ', 11);
                dir[0].name[0] = '.';
                dir[0].attr = ATTR_DIRECTORY;
                dir[0].first_cluster = cluster;
                dir[0].file_size = 0;

                /* ".." entry */
                memset(dir[1].name, ' ', 11);
                dir[1].name[0] = '.';
                dir[1].name[1] = '.';
                dir[1].attr = ATTR_DIRECTORY;
                dir[1].first_cluster = 0;  /* parent = root */
                dir[1].file_size = 0;

                uint32_t dir_sector = cluster_to_sector(cluster);
                disk_write_sector(dir_sector, sector_buf);

                /* Zero out remaining sectors in cluster */
                memset(sector_buf, 0, 512);
                for (int k = 1; k < fs.sectors_per_cluster; k++)
                    disk_write_sector(dir_sector + k, sector_buf);

                return 0;
            }
        }
    }
    /* Root dir full — free the allocated cluster */
    fat_write(cluster, 0x0000);
    return -1;
}

int fs_unlink(const char *path)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    resolve_path(path, &parent, name);
    if (name[0] == '\0') return -1;

    uint32_t sec;
    int idx;
    if (find_root_entry_loc(name, &sec, &idx) != 0)
        return -1;

    /* Read the sector containing the entry */
    if (disk_read_sector(sec, sector_buf) != 0)
        return -1;

    fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
    uint16_t first_cluster = entries[idx].first_cluster;

    /* Mark entry as deleted */
    entries[idx].name[0] = (char)0xE5;
    disk_write_sector(sec, sector_buf);

    /* Free cluster chain */
    if (first_cluster >= 2)
        fat_free_chain(first_cluster);

    return 0;
}

int fs_rename(const char *old_path, const char *new_path)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char old_name[FS_MAX_NAME], new_name[FS_MAX_NAME];
    resolve_path(old_path, &parent, old_name);
    resolve_path(new_path, &parent, new_name);
    if (old_name[0] == '\0' || new_name[0] == '\0') return -1;

    /* Check new name doesn't already exist */
    fat16_dirent_t existing;
    if (find_root_entry(new_name, &existing) == 0)
        return -1;  /* target exists */

    /* Find old entry */
    uint32_t sec;
    int idx;
    if (find_root_entry_loc(old_name, &sec, &idx) != 0)
        return -1;

    /* Read sector, change name, write back */
    if (disk_read_sector(sec, sector_buf) != 0)
        return -1;

    fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
    string_to_fat_name(new_name, entries[idx].name);
    disk_write_sector(sec, sector_buf);

    return 0;
}
