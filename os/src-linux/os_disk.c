/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>    // defines BLKGETSIZE64
#include <sys/types.h>
#include <sys/stat.h>
#include <glob.h>
#include <string.h>
#include <errno.h>

#include "os/include/os_disk.h"
#include "os/include/strlcpy.h"
#include "os/include/os_assert.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"

struct os_disk_iterator
{
    glob_t glob;
    unsigned index;
    bool finished;
};

os_disk_iterator_t *os_disk_iterator_begin(const char *pattern)
{
    os_disk_iterator_t *iter;
    int r;

    OS_ASSERT(pattern != NULL);

    iter = os_malloc(sizeof(os_disk_iterator_t));
    if (iter == NULL)
        return NULL;

    r = glob(pattern, 0, NULL, &iter->glob);
    if (r == GLOB_NOMATCH)
        iter->finished = true;
    else
    if (r != 0)
    {
        os_free(iter);
        return NULL;
    }

    iter->finished = false;
    iter->index = 0;

    return iter;
}

void __os_disk_iterator_end(os_disk_iterator_t **iter)
{
    if (iter == NULL)
        return;

    if (*iter != NULL)
    {
        globfree(&(*iter)->glob);

        os_free(*iter);
        *iter = NULL;
    }
}

static bool __is_block_device(const char *disk)
{
    struct stat st;
    /* if the stat()ing of the disk fails, it is assumed *not* to be
       a block device. */
    return stat(disk, &st) == 0 && S_ISBLK(st.st_mode);
}

const char *os_disk_iterator_get(os_disk_iterator_t *iter)
{
    const char *disk;

    if (iter == NULL)
        return NULL;

    if (iter->index >= iter->glob.gl_pathc)
        iter->finished = true;

    if (iter->finished)
        return NULL;

    disk = iter->glob.gl_pathv[iter->index];
    while (iter->index < iter->glob.gl_pathc && !__is_block_device(disk))
    {
        iter->index++;
        disk = iter->glob.gl_pathv[iter->index];
    }

    iter->finished = (iter->index >= iter->glob.gl_pathc);
    if (iter->finished)
        return NULL;

    /* Set index for next call */
    iter->index++;

    return disk;
}

int os_disk_get_size(int fd, uint64_t *size)
{
    int r;

    if (fd < 0 || size == NULL)
        return -EINVAL;

    r = ioctl(fd, BLKGETSIZE64, size);
    return r < 0 ? -errno : r;
}

int os_disk_open_raw(const char *disk, int oflags)
{
    int posix_flags = 0;
    int r;

    if (disk == NULL)
        return -EINVAL;

    if (oflags == 0)
        return -EINVAL;
    if (oflags & ~(OS_DISK_READ | OS_DISK_WRITE | OS_DISK_DIRECT | OS_DISK_EXCL))
        return -EINVAL;

    if ((oflags & OS_DISK_READ) && (oflags & OS_DISK_WRITE))
        posix_flags = O_RDWR;
    else if (oflags & OS_DISK_READ)
        posix_flags = O_RDONLY;
    else if (oflags & OS_DISK_WRITE)
        posix_flags = O_WRONLY;

    if (oflags & OS_DISK_DIRECT)
        posix_flags |= O_DIRECT;

    if (oflags & OS_DISK_EXCL)
        posix_flags |= O_EXCL;

    r = open(disk, posix_flags);

    return r < 0 ? -errno : r;
}

int os_disk_normalize_path(const char *in_path, char *out_path, size_t out_size)
{
    if (in_path == NULL || out_path == NULL || out_size == 0)
        return -EINVAL;

    /* There's actually nothing to do */
    if (strlcpy(out_path, in_path, out_size) >= out_size)
        return -ENAMETOOLONG;

    return 0;
}

bool os_disk_path_is_valid(const char *path)
{
    if (path == NULL)
        return false;

    return strstr(path, "/dev/") == path;
}

bool os_disk_has_fs(const char *drive)
{
    char cmd[255];
    int ret;

    if (drive == NULL)
	return false;

    os_snprintf(cmd, sizeof(cmd),
                "file -s %s | grep filesystem >/dev/null 2>&1", drive);
    ret = system(cmd);

    return ret == 0;
}
