/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_DISK_H
#define _OS_DISK_H

#include "os/include/os_inttypes.h"

#define OS_DISK_READ    0x01
#define OS_DISK_WRITE   0x02
#define OS_DISK_RDWR    (OS_DISK_READ | OS_DISK_WRITE)
#define OS_DISK_DIRECT  0x04
#define OS_DISK_EXCL    0x08


/** A disk iterator */
typedef struct os_disk_iterator os_disk_iterator_t;

/**
 * Initialize a disk iterator.
 * The iterator will yield only those disks that match the pattern specified.
 *
 * @param[in] pattern  The disk pattern
 *
 * @return Iterator if successful, NULL otherwise
 *
 * @os_replace{Linux, glob}
 * @os_replace{Windows, FindFirstVolume}
 */
os_disk_iterator_t *os_disk_iterator_begin(const char *pattern);

/**
 * Terminate a disk iterator.
 * This function *must* be called when the iterator is no longer needed.
 *
 * @note Don't use this function directly. Use macro os_disk_iterator_end()
 * instead as it is safer.
 *
 * @param iter  Iterator to terminate
 *
 * @os_replace{Linux, globfree}
 * @os_replace{Windows, FindVolumeClose}
 */
void __os_disk_iterator_end(os_disk_iterator_t **iter);

/* Terminate a disk iterator and set it to NULL */
#define os_disk_iterator_end(iter)  __os_disk_iterator_end(&iter)

/**
 * Get a disk from an iterator.
 * The disk returned, if any, matches the iterator's pattern.
 *
 * @param iter  Iterator to get a disk from
 *
 * @return A disk or NULL if there is no more disks matching the pattern
 *
 * @os_replace{Windows, FindNextVolume}
 */
const char *os_disk_iterator_get(os_disk_iterator_t *iter);

/**
 * Open a disk in raw mode.
 *
 * @param[in] disk    Path of the disk to open
 * @param[in] oflags  Flags to open the disk with:
 *     OS_DISK_READ:   allow to read
 *     OS_DISK_WRITE:  allow to write
 *     OS_DISK_RDWR:   allow to read and write.
 *     OS_DISK_DIRECT: bypass the OS cache (and the disk cache on Windows)
 *     OS_DISK_EXCL:   don't share the disk (Linux refuses to open a
 *         mounted disk, Windows refuses to open a disk on which a file
 *         is open).
 *
 * @return File descriptor if successful, a negative error code otherwise
 *
 * @os_replace{Linux, open}
 * @os_replace{Windows, CreateFile, _open_osfhandle}
 */
int os_disk_open_raw(const char *disk, int oflags);

/**
 * Get the size of a given disk (in bytes). The disk must be opened with
 * the OS_DISK_READ or the OS_DISK_RDWR flag.
 *
 * @param[in]  fd    File descriptor to the disk
 * @param[out] size  Disk size (in bytes)
 *
 * @return 0 if successful, a negative error code otherwise
 *
 * @os_replace{Linux, BLKGETSIZE64}
 * @os_replace{Windows, GetFileSizeEx}
 */
int os_disk_get_size(int fd, uint64_t *size);

/**
 * Normalize the path of a disk.
 * The normalization depends on the platform.
 *
 * @param[in]  in_path   Path to normalize
 * @param[out] out_path  Resulting normalized path
 * @param[in]  out_size  Size of resulting path
 *
 * @note You may give the same variable for both in_path and out_path.
 *
 * @note If the input path is not a valid disk path, the output path
 *       won't be valid either. Use os_disk_path_is_valid() to check
 *       the result.
 *
 * @return 0 if successful, a negative error code otherwise
 *         (-EINVAL if any of the parameters is invalid, -ENAMETOOLONG
 *         if the resulting path doesn't fit in out_path)
 */
int os_disk_normalize_path(const char *in_path, char *out_path, size_t out_size);

/**
 * Check whether a disk path is syntactically valid (no verification if it
 * actually exists).
 *
 * @param[in] path  Disk path to check
 *
 * @return true if path is valid, false otherwise
 */
bool os_disk_path_is_valid(const char *path);


/**
 * Check if a given drive has a FS on it or not
 *
 * @param drive   The path of the drive to check
 *
 * @return true if the drive has a FS, false otherwise
 */
bool os_disk_has_fs(const char *drive);

#endif /* _OS_DISK_H */

