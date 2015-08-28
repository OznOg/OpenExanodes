/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_filemap.h"

#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_assert.h"
#include "os/include/os_string.h"
#include "os/include/os_error.h"
#include <sys/mman.h>
#include <sys/stat.h>

struct os_fmap
{
    int    fd;
    char   path[OS_PATH_MAX + 1];
    size_t size;
    void  *addr;
    fmap_access_t access;
};

os_fmap_t *os_fmap_create(const char *path, size_t size)
{
    os_fmap_t *fmap;
    int err;

    fmap = os_malloc(sizeof(os_fmap_t));
    if (fmap == NULL)
        return NULL;

    fmap->fd = open(path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fmap->fd == -1)
    {
        os_free(fmap);
        return NULL;
    }

    /* destroy content of file if it was already existing */
    err = ftruncate(fmap->fd, 0);
    if (err == 0)
        err = ftruncate(fmap->fd, size);

    if (err == -1)
    {
        close(fmap->fd);
        unlink(path);
        os_free(fmap);
        return NULL;
    }

    fmap->size = size;
    fmap->access = FMAP_RDWR;

    os_strlcpy(fmap->path, path, sizeof(fmap->path));

    /* The MAP_POPULATE flag is here to force the kernel to allocate all
     * mapping pages at creation. This (should) affords that there cannot
     * be memory allocations during recovery when accessing the file mapping,
     * anf thus should fix bug #4557.
     * FIXME Check this flag really solves the problem (but this is not really
     * easy...). */
    fmap->addr = mmap(NULL, size, PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE | MAP_LOCKED | MAP_NORESERVE,
                      fmap->fd, 0);

    if (fmap->addr == MAP_FAILED)
    {
        close(fmap->fd);
        unlink(path);
        os_free(fmap);
        return NULL;
    }

    return fmap;
}

os_fmap_t *os_fmap_open(const char *path, size_t size, fmap_access_t access)
{
    struct stat stat;
    os_fmap_t *fmap;
    int file_mode = 0, map_mode = 0;

    fmap = os_malloc(sizeof(os_fmap_t));
    if (fmap == NULL)
        return NULL;

    OS_ASSERT(FMAP_ACCESS_IS_VALID(access));

    fmap->access = access;

    switch(access)
    {
    case FMAP_READ:
        file_mode = S_IRUSR;
        map_mode = PROT_READ;
        break;
    case FMAP_WRITE:
        file_mode = S_IWUSR;
        map_mode = PROT_WRITE;
        break;
    case FMAP_RDWR:
        file_mode = S_IRUSR | S_IWUSR;
        map_mode = PROT_READ | PROT_WRITE;
        break;
    }

    fmap->fd = open(path, O_RDWR, file_mode);
    if (fmap->fd == -1)
    {
        os_free(fmap);
        return NULL;
    }

    if (fstat(fmap->fd, &stat) < 0 || stat.st_size != size)
    {
        close(fmap->fd);
        os_free(fmap);
        return NULL;
    }

    fmap->addr = mmap(NULL, size, map_mode,
                      MAP_SHARED | MAP_LOCKED | MAP_NORESERVE, fmap->fd, 0);

    if (fmap->addr == MAP_FAILED)
    {
        close(fmap->fd);
        unlink(path);
        os_free(fmap);
        return NULL;
    }

    fmap->size = size;
    os_strlcpy(fmap->path, path, sizeof(fmap->path));

    return fmap;
}

static void __os_fmap_unmap_close(os_fmap_t *fmap)
{
    int err;
    OS_ASSERT(fmap != NULL);
    OS_ASSERT(fmap->fd >= 0);

    if (fmap->access != FMAP_READ)
    {
        err = msync(fmap->addr, fmap->size, MS_SYNC);
        OS_ASSERT(err == 0);
    }

    err = munmap(fmap->addr, fmap->size);
    OS_ASSERT(err == 0);

    fmap->addr = NULL;

    err = close(fmap->fd);
    OS_ASSERT(err == 0);
}

void os_fmap_close(os_fmap_t *fmap)
{
    __os_fmap_unmap_close(fmap);

    os_free(fmap);
}

int os_fmap_delete(os_fmap_t *fmap)
{
    int err;

    if (fmap->access == FMAP_READ)
        return -EPERM;

    __os_fmap_unmap_close(fmap);

    err = unlink(fmap->path);

    os_free(fmap);

    return err;
}

int os_fmap_write(const os_fmap_t *fmap, size_t offset,
                  const void *in, size_t size)
{
    if (fmap == NULL || in == NULL || size + offset > fmap->size)
        return -EINVAL;

    if (fmap->access == FMAP_READ)
        return -EPERM;

    memcpy((char *)fmap->addr + offset, in, size);

    if (msync(fmap->addr, fmap->size, MS_SYNC))
        return -errno;

    return size;
}

int os_fmap_read(const os_fmap_t *fmap, size_t offset, void *out, size_t size)
{
    if (fmap == NULL || out == NULL || size + offset > fmap->size)
        return -EINVAL;

    memcpy(out, (char *)fmap->addr + offset, size);

    return size;
}

void *os_fmap_addr(const os_fmap_t *fmap)
{
    return fmap == NULL ? NULL : fmap->addr;
}

size_t os_fmap_size(const os_fmap_t *fmap)
{
    return fmap == NULL ? -1 : fmap->size;
}

fmap_access_t os_fmap_access(const os_fmap_t *fmap)
{
    return fmap == NULL ? - 1 : fmap->access;
}

int os_fmap_sync(const os_fmap_t *fmap)
{
    if (fmap == NULL)
        return -EINVAL;

    if (fmap->access == FMAP_READ)
        return -EPERM;

    if (msync(fmap->addr, fmap->size, MS_SYNC) != 0)
        return -errno;

    return 0;
}
