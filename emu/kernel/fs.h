#ifndef KERNEL_FS_H
#define KERNEL_FS_H

#include <stdint.h>

#define FS_MAX_PATH     256
#define FS_MAX_NAME     64
#define FS_MAX_OPEN     16

/* File types */
#define FS_TYPE_FILE    1
#define FS_TYPE_DIR     2

/* Open flags */
#define FS_O_READ       (1 << 0)
#define FS_O_WRITE      (1 << 1)
#define FS_O_CREATE     (1 << 2)
#define FS_O_TRUNC      (1 << 3)

typedef struct {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  type;       /* FS_TYPE_FILE or FS_TYPE_DIR */
} fs_stat_t;

typedef struct {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  type;
} fs_dirent_t;

void fs_init(void);
int  fs_open(const char *path, int flags);
int  fs_close(int fd);
int  fs_read(int fd, void *buf, int size);
int  fs_write(int fd, const void *buf, int size);
int  fs_stat(const char *path, void *stat_out);
int  fs_readdir(int fd, void *entry_out);
int  fs_mkdir(const char *path);
int  fs_unlink(const char *path);
int  fs_rename(const char *old_path, const char *new_path);

#endif
