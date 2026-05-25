#include "fs.h"
#include "disk.h"
#include "string.h"
#include "kprintf.h"

/* ═══════════════════════════════════════════════════════════════════
   FAT16 filesystem implementation — with subdirectory support
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
    uint16_t parent_cluster;    /* cluster of containing directory, 0=root */
    char     fname[FS_MAX_NAME]; /* just the filename component */
    uint32_t dir_sector;        /* for readdir: current sector in dir */
    int      dir_entry;         /* for readdir: entry index within sector */
    int      dir_total_entries; /* total dir entries to iterate */
    char     path[FS_MAX_PATH];
} fd_entry_t;

static fd_entry_t fd_table[FS_MAX_OPEN];

/* Sector buffer */
static uint8_t sector_buf[512];

/* ─── Low-level FAT helpers ─────────────────────────────────────── */

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

/* ─── Name conversion helpers ───────────────────────────────────── */

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

/* ─── Directory search (works in root or any subdirectory) ──────── */

/*
 * dir_cluster = 0  → search the root directory (fixed-size area)
 * dir_cluster >= 2 → search a subdirectory (cluster chain)
 */
static int find_entry_in_dir(uint16_t dir_cluster, const char *name,
                              fat16_dirent_t *out)
{
    char fat_name[11];
    string_to_fat_name(name, fat_name);

    if (dir_cluster == 0) {
        /* Root directory — fixed area */
        for (uint32_t s = 0; s < fs.root_sectors; s++) {
            if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
                return -1;
            fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
            int per_sector = 512 / (int)sizeof(fat16_dirent_t);
            for (int i = 0; i < per_sector; i++) {
                if (entries[i].name[0] == 0x00) return -1;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr & ATTR_VOLUME_ID) continue;
                if (entries[i].attr == ATTR_LFN) continue;
                if (memcmp(entries[i].name, fat_name, 11) == 0) {
                    memcpy(out, &entries[i], sizeof(fat16_dirent_t));
                    return 0;
                }
            }
        }
    } else {
        /* Subdirectory — follow cluster chain */
        uint16_t clus = dir_cluster;
        while (clus >= 2 && clus < 0xFFF8) {
            uint32_t base = cluster_to_sector(clus);
            for (int s = 0; s < fs.sectors_per_cluster; s++) {
                if (disk_read_sector(base + s, sector_buf) != 0)
                    return -1;
                fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
                int per_sector = 512 / (int)sizeof(fat16_dirent_t);
                for (int i = 0; i < per_sector; i++) {
                    if (entries[i].name[0] == 0x00) return -1;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                    if (entries[i].attr & ATTR_VOLUME_ID) continue;
                    if (entries[i].attr == ATTR_LFN) continue;
                    if (memcmp(entries[i].name, fat_name, 11) == 0) {
                        memcpy(out, &entries[i], sizeof(fat16_dirent_t));
                        return 0;
                    }
                }
            }
            clus = fat_read(clus);
        }
    }
    return -1;
}

/* Find entry location (sector + index) for in-place modification */
static int find_entry_in_dir_loc(uint16_t dir_cluster, const char *name,
                                  uint32_t *out_sector, int *out_idx)
{
    char fat_name[11];
    string_to_fat_name(name, fat_name);

    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fs.root_sectors; s++) {
            if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
                return -1;
            fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
            int per_sector = 512 / (int)sizeof(fat16_dirent_t);
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
    } else {
        uint16_t clus = dir_cluster;
        while (clus >= 2 && clus < 0xFFF8) {
            uint32_t base = cluster_to_sector(clus);
            for (int s = 0; s < fs.sectors_per_cluster; s++) {
                if (disk_read_sector(base + s, sector_buf) != 0)
                    return -1;
                fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
                int per_sector = 512 / (int)sizeof(fat16_dirent_t);
                for (int i = 0; i < per_sector; i++) {
                    if (entries[i].name[0] == 0x00) return -1;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                    if (entries[i].attr & ATTR_VOLUME_ID) continue;
                    if (entries[i].attr == ATTR_LFN) continue;
                    if (memcmp(entries[i].name, fat_name, 11) == 0) {
                        *out_sector = base + s;
                        *out_idx = i;
                        return 0;
                    }
                }
            }
            clus = fat_read(clus);
        }
    }
    return -1;
}

/* Find a free slot in a directory. Returns sector + index. */
static int find_free_in_dir(uint16_t dir_cluster,
                             uint32_t *out_sector, int *out_idx)
{
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fs.root_sectors; s++) {
            if (disk_read_sector(fs.root_start + s, sector_buf) != 0)
                return -1;
            fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
            int per_sector = 512 / 32;
            for (int i = 0; i < per_sector; i++) {
                if (entries[i].name[0] == 0x00 ||
                    (uint8_t)entries[i].name[0] == 0xE5) {
                    *out_sector = fs.root_start + s;
                    *out_idx = i;
                    return 0;
                }
            }
        }
    } else {
        uint16_t clus = dir_cluster;
        while (clus >= 2 && clus < 0xFFF8) {
            uint32_t base = cluster_to_sector(clus);
            for (int s = 0; s < fs.sectors_per_cluster; s++) {
                if (disk_read_sector(base + s, sector_buf) != 0)
                    return -1;
                fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
                int per_sector = 512 / 32;
                for (int i = 0; i < per_sector; i++) {
                    if (entries[i].name[0] == 0x00 ||
                        (uint8_t)entries[i].name[0] == 0xE5) {
                        *out_sector = base + s;
                        *out_idx = i;
                        return 0;
                    }
                }
            }
            clus = fat_read(clus);
        }
    }
    return -1;  /* directory full */
}

/* ─── Path resolution ───────────────────────────────────────────── */

/*
 * Walk a path like "/docs/sub/file.txt":
 *   parent_cluster = cluster of "sub" (the parent dir)
 *   final_name     = "file.txt"
 *
 * For "/docs":
 *   parent_cluster = 0 (root)
 *   final_name     = "docs"
 *
 * For "/":
 *   parent_cluster = 0
 *   final_name     = "" (empty → means root itself)
 */
static int resolve_path(const char *path, uint16_t *parent_cluster,
                         char *final_name)
{
    /* Skip leading slashes */
    while (*path == '/') path++;
    if (*path == '\0') {
        *parent_cluster = 0;
        final_name[0] = '\0';
        return 0;
    }

    uint16_t cur_dir = 0;  /* start at root */

    /* Walk path components */
    while (1) {
        /* Extract next component */
        const char *slash = strchr(path, '/');
        int comp_len;
        if (slash) {
            comp_len = (int)(slash - path);
        } else {
            /* This is the final component */
            comp_len = (int)strlen(path);
            if (comp_len >= FS_MAX_NAME) comp_len = FS_MAX_NAME - 1;
            memcpy(final_name, path, comp_len);
            final_name[comp_len] = '\0';
            *parent_cluster = cur_dir;
            return 0;
        }

        /* Intermediate component — must be a directory */
        char comp[FS_MAX_NAME];
        if (comp_len >= FS_MAX_NAME) comp_len = FS_MAX_NAME - 1;
        memcpy(comp, path, comp_len);
        comp[comp_len] = '\0';

        fat16_dirent_t entry;
        if (find_entry_in_dir(cur_dir, comp, &entry) != 0)
            return -1;  /* component not found */
        if (!(entry.attr & ATTR_DIRECTORY))
            return -1;  /* not a directory */

        cur_dir = entry.first_cluster;
        path = slash + 1;
        /* Skip consecutive slashes */
        while (*path == '/') path++;
        if (*path == '\0') {
            /* Trailing slash — treat dir itself as target */
            *parent_cluster = cur_dir;
            final_name[0] = '\0';
            return 0;
        }
    }
}

/* ─── Filesystem init ───────────────────────────────────────────── */

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

/* ─── fs_open ───────────────────────────────────────────────────── */

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
    if (resolve_path(path, &parent, name) != 0)
        return -1;

    if (name[0] == '\0') {
        /* Opening a directory directly (root or subdirectory via trailing path) */
        fd_table[fd].used = 1;
        fd_table[fd].start_cluster = parent;
        fd_table[fd].cur_cluster = parent;
        fd_table[fd].file_size = 0;
        fd_table[fd].position = 0;
        fd_table[fd].is_dir = 1;
        fd_table[fd].parent_cluster = 0;
        fd_table[fd].fname[0] = '\0';
        if (parent == 0) {
            fd_table[fd].dir_sector = fs.root_start;
            fd_table[fd].dir_entry = 0;
            fd_table[fd].dir_total_entries = fs.root_entry_count;
        } else {
            fd_table[fd].dir_sector = cluster_to_sector(parent);
            fd_table[fd].dir_entry = 0;
            fd_table[fd].dir_total_entries = fs.sectors_per_cluster * 16;
        }
        strncpy(fd_table[fd].path, path, FS_MAX_PATH - 1);
        return fd;
    }

    fat16_dirent_t entry;
    if (find_entry_in_dir(parent, name, &entry) == 0) {
        fd_table[fd].used = 1;
        fd_table[fd].start_cluster = entry.first_cluster;
        fd_table[fd].cur_cluster = entry.first_cluster;
        fd_table[fd].file_size = entry.file_size;
        fd_table[fd].position = 0;
        fd_table[fd].is_dir = (entry.attr & ATTR_DIRECTORY) ? 1 : 0;
        fd_table[fd].parent_cluster = parent;
        strncpy(fd_table[fd].fname, name, FS_MAX_NAME - 1);
        fd_table[fd].fname[FS_MAX_NAME - 1] = '\0';
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
        uint32_t sec;
        int idx;
        if (find_free_in_dir(parent, &sec, &idx) != 0)
            return -1;  /* directory full */

        if (disk_read_sector(sec, sector_buf) != 0)
            return -1;
        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        string_to_fat_name(name, entries[idx].name);
        entries[idx].attr = ATTR_ARCHIVE;
        entries[idx].first_cluster = 0;
        entries[idx].file_size = 0;
        memset(entries[idx].reserved, 0, 10);
        entries[idx].time = 0;
        entries[idx].date = 0;
        disk_write_sector(sec, sector_buf);

        fd_table[fd].used = 1;
        fd_table[fd].start_cluster = 0;
        fd_table[fd].cur_cluster = 0;
        fd_table[fd].file_size = 0;
        fd_table[fd].position = 0;
        fd_table[fd].is_dir = 0;
        fd_table[fd].parent_cluster = parent;
        strncpy(fd_table[fd].fname, name, FS_MAX_NAME - 1);
        fd_table[fd].fname[FS_MAX_NAME - 1] = '\0';
        strncpy(fd_table[fd].path, path, FS_MAX_PATH - 1);
        return fd;
    }

    return -1;  /* not found */
}

/* ─── fs_close ──────────────────────────────────────────────────── */

int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN || !fd_table[fd].used)
        return -1;
    fd_table[fd].used = 0;
    return 0;
}

/* ─── fs_read ───────────────────────────────────────────────────── */

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

/* ─── fs_write ──────────────────────────────────────────────────── */

int fs_write(int fd, const void *buf, int size)
{
    if (fd < 0 || fd >= FS_MAX_OPEN || !fd_table[fd].used)
        return -1;
    if (fd_table[fd].is_dir) return -1;
    if (size <= 0) return 0;

    fd_entry_t *f = &fd_table[fd];
    const uint8_t *src = (const uint8_t *)buf;
    int total_written = 0;

    /* If file has no cluster yet, allocate one */
    if (f->start_cluster == 0) {
        uint16_t c = fat_alloc();
        if (c == 0) return -1;
        f->start_cluster = c;
        f->cur_cluster = c;

        /* Update directory entry with new cluster */
        if (f->fname[0] != '\0') {
            uint32_t sec;
            int idx;
            if (find_entry_in_dir_loc(f->parent_cluster, f->fname, &sec, &idx) == 0) {
                if (disk_read_sector(sec, sector_buf) == 0) {
                    fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
                    entries[idx].first_cluster = c;
                    disk_write_sector(sec, sector_buf);
                }
            }
        }
    }

    while (size > 0) {
        uint32_t cluster_size = (uint32_t)fs.sectors_per_cluster * 512;
        uint32_t pos_in_cluster = f->position % cluster_size;
        uint32_t sector_in_cluster = pos_in_cluster / 512;
        uint32_t pos_in_sector = pos_in_cluster % 512;

        uint32_t sector = cluster_to_sector(f->cur_cluster) + sector_in_cluster;

        /* Read existing sector data (for partial writes) */
        if (pos_in_sector != 0 || size < 512) {
            if (disk_read_sector(sector, sector_buf) != 0)
                break;
        } else {
            memset(sector_buf, 0, 512);
        }

        /* Write into sector buffer */
        int avail = 512 - (int)pos_in_sector;
        int to_write = size < avail ? size : avail;
        memcpy(sector_buf + pos_in_sector, src, to_write);

        if (disk_write_sector(sector, sector_buf) != 0)
            break;

        src += to_write;
        f->position += to_write;
        size -= to_write;
        total_written += to_write;

        /* Update file size if we wrote past the end */
        if (f->position > f->file_size)
            f->file_size = f->position;

        /* Need next cluster? */
        if (f->position % cluster_size == 0 && size > 0) {
            uint16_t next = fat_read(f->cur_cluster);
            if (next >= 0xFFF8 || next < 2) {
                /* Allocate new cluster */
                uint16_t nc = fat_alloc();
                if (nc == 0) break;
                fat_write(f->cur_cluster, nc);
                f->cur_cluster = nc;
            } else {
                f->cur_cluster = next;
            }
        }
    }

    /* Update directory entry size */
    if (total_written > 0 && f->fname[0] != '\0') {
        uint32_t sec;
        int idx;
        if (find_entry_in_dir_loc(f->parent_cluster, f->fname, &sec, &idx) == 0) {
            if (disk_read_sector(sec, sector_buf) == 0) {
                fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
                entries[idx].file_size = f->file_size;
                disk_write_sector(sec, sector_buf);
            }
        }
    }

    return total_written;
}

/* ─── fs_stat ───────────────────────────────────────────────────── */

int fs_stat(const char *path, void *stat_out)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    if (resolve_path(path, &parent, name) != 0)
        return -1;

    fs_stat_t *st = (fs_stat_t *)stat_out;

    if (name[0] == '\0') {
        /* Root directory (or subdir accessed via trailing slash) */
        strcpy(st->name, "/");
        st->size = 0;
        st->type = FS_TYPE_DIR;
        return 0;
    }

    fat16_dirent_t entry;
    if (find_entry_in_dir(parent, name, &entry) == 0) {
        fat_name_to_string(entry.name, st->name);
        st->size = entry.file_size;
        st->type = (entry.attr & ATTR_DIRECTORY) ? FS_TYPE_DIR : FS_TYPE_FILE;
        return 0;
    }

    return -1;
}

/* ─── fs_readdir ────────────────────────────────────────────────── */

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

/* ─── fs_mkdir ──────────────────────────────────────────────────── */

int fs_mkdir(const char *path)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    if (resolve_path(path, &parent, name) != 0)
        return -1;
    if (name[0] == '\0') return -1;  /* can't mkdir root */

    /* Check if already exists */
    fat16_dirent_t existing;
    if (find_entry_in_dir(parent, name, &existing) == 0)
        return -1;  /* already exists */

    /* Allocate a cluster for the directory data */
    uint16_t cluster = fat_alloc();
    if (cluster == 0) return -1;

    /* Find an empty entry in parent directory */
    uint32_t sec;
    int idx;
    if (find_free_in_dir(parent, &sec, &idx) != 0) {
        fat_write(cluster, 0x0000);
        return -1;  /* parent dir full */
    }

    if (disk_read_sector(sec, sector_buf) != 0) {
        fat_write(cluster, 0x0000);
        return -1;
    }
    fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
    string_to_fat_name(name, entries[idx].name);
    entries[idx].attr = ATTR_DIRECTORY;
    entries[idx].first_cluster = cluster;
    entries[idx].file_size = 0;
    memset(entries[idx].reserved, 0, 10);
    entries[idx].time = 0;
    entries[idx].date = 0;
    disk_write_sector(sec, sector_buf);

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
    dir[1].first_cluster = parent;  /* parent cluster (0 = root) */
    dir[1].file_size = 0;

    uint32_t dir_sector = cluster_to_sector(cluster);
    disk_write_sector(dir_sector, sector_buf);

    /* Zero out remaining sectors in cluster */
    memset(sector_buf, 0, 512);
    for (int k = 1; k < fs.sectors_per_cluster; k++)
        disk_write_sector(dir_sector + k, sector_buf);

    return 0;
}

/* ─── fs_unlink ─────────────────────────────────────────────────── */

int fs_unlink(const char *path)
{
    if (!fs.mounted) return -1;

    uint16_t parent;
    char name[FS_MAX_NAME];
    if (resolve_path(path, &parent, name) != 0)
        return -1;
    if (name[0] == '\0') return -1;

    uint32_t sec;
    int idx;
    if (find_entry_in_dir_loc(parent, name, &sec, &idx) != 0)
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

/* ─── fs_rename ─────────────────────────────────────────────────── */

int fs_rename(const char *old_path, const char *new_path)
{
    if (!fs.mounted) return -1;

    uint16_t old_parent, new_parent;
    char old_name[FS_MAX_NAME], new_name[FS_MAX_NAME];
    if (resolve_path(old_path, &old_parent, old_name) != 0)
        return -1;
    if (resolve_path(new_path, &new_parent, new_name) != 0)
        return -1;
    if (old_name[0] == '\0' || new_name[0] == '\0') return -1;

    if (old_parent == new_parent) {
        /* Same directory — simple rename */
        /* Check new name doesn't already exist */
        fat16_dirent_t existing;
        if (find_entry_in_dir(old_parent, new_name, &existing) == 0)
            return -1;  /* target exists */

        uint32_t sec;
        int idx;
        if (find_entry_in_dir_loc(old_parent, old_name, &sec, &idx) != 0)
            return -1;

        if (disk_read_sector(sec, sector_buf) != 0)
            return -1;

        fat16_dirent_t *entries = (fat16_dirent_t *)sector_buf;
        string_to_fat_name(new_name, entries[idx].name);
        disk_write_sector(sec, sector_buf);
        return 0;
    }

    /* Cross-directory move: copy entry to new dir, delete from old */
    fat16_dirent_t entry;
    if (find_entry_in_dir(old_parent, old_name, &entry) != 0)
        return -1;

    /* Check new name doesn't already exist */
    fat16_dirent_t existing;
    if (find_entry_in_dir(new_parent, new_name, &existing) == 0)
        return -1;

    /* Find a free slot in new directory */
    uint32_t new_sec;
    int new_idx;
    if (find_free_in_dir(new_parent, &new_sec, &new_idx) != 0)
        return -1;

    /* Write entry to new location */
    if (disk_read_sector(new_sec, sector_buf) != 0)
        return -1;
    fat16_dirent_t *new_entries = (fat16_dirent_t *)sector_buf;
    memcpy(&new_entries[new_idx], &entry, sizeof(fat16_dirent_t));
    string_to_fat_name(new_name, new_entries[new_idx].name);
    disk_write_sector(new_sec, sector_buf);

    /* Delete from old location */
    uint32_t old_sec;
    int old_idx;
    if (find_entry_in_dir_loc(old_parent, old_name, &old_sec, &old_idx) != 0)
        return -1;
    if (disk_read_sector(old_sec, sector_buf) != 0)
        return -1;
    fat16_dirent_t *old_entries = (fat16_dirent_t *)sector_buf;
    old_entries[old_idx].name[0] = (char)0xE5;
    disk_write_sector(old_sec, sector_buf);

    return 0;
}
