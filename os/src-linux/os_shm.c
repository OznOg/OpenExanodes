/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <sys/mman.h>
#include <sys/stat.h>  /* for mode_t */
#include <fcntl.h>     /* for O_* constants */
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "os/include/os_assert.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "os/include/os_shm.h"

/* The full id of a shm is prepended with '/' */
#define FULL_ID_LEN  (1 + OS_SHM_ID_MAXLEN)

typedef char os_shm_id_t[FULL_ID_LEN + 1];

struct os_shm {
   int         fd;        /* fd of the share memory */
   os_shm_id_t id;
   bool        owner;
   size_t      size;      /* amount of data shared */
   void        *data;     /* data shared */
};


static int mk_valid_shm_id(char *shm_id, const char *id)
{
    size_t n;

    /* the given id MUST NOT begin with / */
    OS_ASSERT(id[0] != '/');

    /* a valid id for shm_open must begin with '/', so add it;
     * this is os dependant that's why it is done here */
    shm_id[0] = '/';

    /* The user-provided 'id' has length OS_SHM_ID_MAXLEN max,
     * which is less than FULL_ID_LEN */
    n = strlcpy(shm_id + 1, id, OS_SHM_ID_MAXLEN);
    if (n >= OS_SHM_ID_MAXLEN)
        return -EINVAL;
    shm_id[n] = '\0';

    return 0;
}

os_shm_t *os_shm_create(const char *id, size_t size)
{
    os_shm_id_t _id;
    os_shm_t *shm;
    int err;

    if (id == NULL || strlen(id) >= OS_SHM_ID_MAXLEN)
        return NULL;

    if (size == 0)
        return NULL;

    if (mk_valid_shm_id(_id, id) < 0)
        return NULL;

    shm = os_malloc(sizeof(os_shm_t));
    if (shm == NULL)
    {
        printf("os_malloc() failed\n");
	return NULL;
    }

    strcpy(shm->id, _id);
    shm->fd = -1;

    /* if the shm already exists, we destroy it */
    if (shm_unlink(shm->id) == -1 && errno != ENOENT)
    {
        printf("shm_unlik() failed with %d\n", errno);
        goto failed;
    }

    shm->fd = shm_open(shm->id, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    /* FIXME O_EXCL flags should be used, but it is difficult since our
     * daemons do not stop properly ...*/

    if (shm->fd == -1)
    {
        printf("shm_open() failed with %d\n", errno);
        goto failed;
    }

    shm->size = size;
    shm->owner = true;

    /* Set size of the newly created object */
    err = ftruncate(shm->fd, shm->size);
    if (err)
    {
        printf("ftruncate() failed with %d\n", errno);
        goto failed;
    }

    /* Map the shared memory into our address space for use */
    shm->data = mmap(NULL, shm->size, PROT_WRITE | PROT_READ,
	             MAP_SHARED, shm->fd, 0);

    if (shm->data == MAP_FAILED)
    {
        printf("mmap() failed with %d\n", errno);
        goto failed;
    }

    return shm;

failed:
    if (shm != NULL)
    {
        if (shm->fd >= 0)
            close(shm->fd);
        os_free(shm);
    }

    return NULL;
}


void os_shm_delete(os_shm_t *shm)
{
    /* Simple user MUST use release and not delete */
    OS_ASSERT(shm->owner);

    munmap(shm->data, shm->size);
    shm_unlink(shm->id);
    close(shm->fd);
    os_free(shm);
}

os_shm_t *os_shm_get(const char *id, size_t size)
{
    os_shm_id_t _id;
    os_shm_t *shm;

    if (id == NULL || strlen(id) >= OS_SHM_ID_MAXLEN)
        return NULL;

    if (size == 0)
        return NULL;

    if (mk_valid_shm_id(_id, id) < 0)
        return NULL;

    shm = os_malloc(sizeof(os_shm_t));
    if (shm == NULL)
	return NULL;

    strcpy(shm->id, _id);

    shm->fd = shm_open(shm->id, O_RDWR, S_IRUSR);
    if (shm->fd == -1)
    {
	os_free(shm);
	return NULL;
    }

    shm->size = size;
    shm->owner = false;

    /* Map the shared memory into our address space for use */
    shm->data = mmap(NULL, shm->size, PROT_WRITE | PROT_READ,
	             MAP_SHARED, shm->fd, 0);

    if (shm->data == MAP_FAILED)
    {
	close(shm->fd);
	os_free(shm);
	shm = NULL;
    }

    return shm;
}

void os_shm_release(os_shm_t *shm)
{
    /* Owner MUST use delete and not release */
    OS_ASSERT(!shm->owner);
    munmap(shm->data, shm->size);
    close(shm->fd);
    os_free(shm);
}

void *os_shm_get_data(os_shm_t *shm)
{
   return shm->data;
}

