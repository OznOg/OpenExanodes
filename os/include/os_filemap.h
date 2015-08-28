/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_inttypes.h"

typedef struct os_fmap os_fmap_t;

typedef enum {
#define FMAP_ACCESS_FIRST FMAP_READ
    FMAP_READ = 88,
    FMAP_WRITE,
    FMAP_RDWR
#define FMAP_ACCESS_LAST FMAP_RDWR
}
fmap_access_t;

#define FMAP_ACCESS_IS_VALID(x) ((x) >= FMAP_ACCESS_FIRST && (x) <= FMAP_ACCESS_LAST)

/**
 * Create a memory mapping of a file.
 * If the file does not exist, it is created. If the file exists it is
 * overwritten (previous one is erased).
 *
 * @param[in]   path  path to the file to map
 * @param[in]   size  size of the memory mapping
 *
 * @return a handle on a file mapping.
 */
os_fmap_t *os_fmap_create(const char *path, size_t size);


/**
 * Delete a file mapping created with os_fmap_create
 * Upon return handle is invalid and should not be reused, even if an
 * error is returned.
 * The file that was linked to the fmap is also deleted from disk.
 *
 * @param[in]  fmap  file mapping handle.
 *
 * @return 0 on success or a negative error code.
 */
int os_fmap_delete(os_fmap_t *fmap);

/**
 * Open a memory mapping of a file.
 * The file MUST exist and its size must match the size param passed.
 *
 * @param[in]   path    path to the file to map
 * @param[in]   size    size of the memory mapping
 * @param[in]   access  the access mode of the fmap (FMAP_READ, FMAP_WRITE or
 *                      FMAP_RDWR.
 *
 * NOTE: Trying to write to read-only mmaped pages causes SIGSEGV, not the
 * nice return of an error.
 *
 * @return a handle on a file mapping.
 */
os_fmap_t *os_fmap_open(const char *path, size_t size, fmap_access_t access);

/**
 * CLose a file mapping
 * Upon return handle is invalid and should not be reused but files it
 * was related to remains valid on disk.
 *
 * @param[in]  fmap  file mapping handle.
 */
void os_fmap_close(os_fmap_t *fmap);

/**
 * Read data from a memory mapped file.
 *
 * @param[in]       fmap    file mapping handle.
 * @param[in]       offset  offset where to write data.
 * @param[in,out]   out     buffer where to store data read.
 * @param[in]       size    size of data to write.
 *
 * @return size on success or a negative error code.
 */
int os_fmap_read(const os_fmap_t *fmap, size_t offset, void *out, size_t size);

/**
 * Write into a memory mapped file.
 *
 * @param[in]   fmap    file mapping handle.
 * @param[in]   offset  offset where to write data.
 * @param[in]   in      buffer where to store data read.
 * @param[in]   size    size of data to write.
 *
 * @return size on success or a negative error code.
 */
int os_fmap_write(const os_fmap_t *fmap, size_t offset, const void *in, size_t size);

/**
 * Get the readable/writable address of a memory-mapped file.
 *
 * @param[in] fmap  file mapping handle
 *
 * @return address if successful, NULL otherwise
 */
void *os_fmap_addr(const os_fmap_t *fmap);

/**
 * Get the size of a memory-mapped file.
 *
 * @param[in] fmap  file mapping handle
 *
 * @return size if successful, -1 otherwise
 */
size_t os_fmap_size(const os_fmap_t *fmap);

/**
 * Get the access mode of a memory-mapped file.
 *
 * @param[in] fmap  file mapping handle
 *
 * @return access mode if successful, -1 otherwise
 */
fmap_access_t os_fmap_access(const os_fmap_t *fmap);

/**
 * Synchronize in-memory and on-disk versions of a memory-mapped file
 * (ensures the in-memory data is flushed to disk).
 *
 * @param[in] fmap  file mapping handle
 *
 * @return 0 on success, a negative error code otherwise
 */
int os_fmap_sync(const os_fmap_t *fmap);
