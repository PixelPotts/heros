#include "hal_fs.h"

int hal_fs_open(const char *path, int flags)
{
    return fs_open(path, flags);
}

int hal_fs_close(int fd)
{
    return fs_close(fd);
}

int hal_fs_read(int fd, void *buf, int size)
{
    return fs_read(fd, buf, size);
}

int hal_fs_write(int fd, const void *buf, int size)
{
    return fs_write(fd, buf, size);
}

int hal_fs_stat(const char *path, fs_stat_t *st)
{
    return fs_stat(path, (void *)st);
}

int hal_fs_readdir(int fd, fs_dirent_t *entry)
{
    return fs_readdir(fd, (void *)entry);
}

int hal_fs_mkdir(const char *path)
{
    return fs_mkdir(path);
}

int hal_fs_unlink(const char *path)
{
    return fs_unlink(path);
}

int hal_fs_rename(const char *old_path, const char *new_path)
{
    return fs_rename(old_path, new_path);
}
