/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __OS_SHM_H
#define __OS_SHM_H

#include <stdlib.h>

struct os_shm;
typedef struct os_shm os_shm_t;

/* Max length of a shared memory identifier. The value 62 is not a typo, it's
 * intentional (implementation constraints). */
#define OS_SHM_ID_MAXLEN 62

/**
 * Create a shared memory.
 *
 * WARNING If the shared mem with the specified id already exists,
 *         it will be reset.
 *
 * WARNING This function ALLOCATES MEMORY.
 *
 * @param id     name of the shm (must not be NULL
 *               and length MUST be <= OS_SHM_ID_MAXLEN)
 * @param size   size of the shm (must be > 0)
 *
 * @return a shm_descriptor, or NULL.
 *
 * @os_replace{Linux, shm_open, mmap}
 * @os_replace{Windows, CreateFileMapping, MapViewOfFile}
 */
os_shm_t *os_shm_create(const char *id, size_t size);

/**
 * Delete a shm that was created with os_shm_create
 *
 * Only the creator of the shm can and *must* use this function.
 *
 * @param shm The shm to delete
 *
 * @os_replace{Linux, munmap, shm_unlink}
 * @os_replace{Windows, UnmapViewOfFile}
 */
void os_shm_delete(os_shm_t *shm);

/**
 * Get a descriptor on an already existent shm
 *
 * WARNING   This function ALLOCATES MEMORY
 *
 * @param id    The id of the shm (must not be NULL)
 * @param size  The size of the shm (must be > 0)
 *
 * @return NULL if the shm does not exist
 *
 * @os_replace{Linux, shm_open, mmap}
 * @os_replace{Windows, OpenFileMapping, MapViewOfFile}
 */
os_shm_t *os_shm_get(const char *id, size_t size);

/**
 * Release a shm allocated with os_shm_get
 *
 * Each user (non-creator) of the shm can and *must* use this function.
 *
 * @param shm  The shm to release
 *
 * @os_replace{Linux, munmap}
 * @os_replace{Windows, UnmapViewOfFile}
 */
void os_shm_release(os_shm_t *shm);

/**
 * Get a pointer on the shared data
 *
 * @param shm  The shared data pointer
 */
void *os_shm_get_data(os_shm_t *shm);

#endif
