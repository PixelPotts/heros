#ifndef HAL_FS_H
#define HAL_FS_H

#include "../kernel/fs.h"

/* Thin wrappers around kernel fs API */
int  hal_fs_open(const char *path, int flags);
int  hal_fs_close(int fd);
int  hal_fs_read(int fd, void *buf, int size);
int  hal_fs_write(int fd, const void *buf, int size);
int  hal_fs_stat(const char *path, fs_stat_t *st);
int  hal_fs_readdir(int fd, fs_dirent_t *entry);
int  hal_fs_mkdir(const char *path);
int  hal_fs_unlink(const char *path);
int  hal_fs_rename(const char *old_path, const char *new_path);

#endif
