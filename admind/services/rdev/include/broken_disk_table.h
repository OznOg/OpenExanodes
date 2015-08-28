/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __BROKEN_DISK_TABLE_H
#define __BROKEN_DISK_TABLE_H

#include "common/include/uuid.h"

/** A table of broken disks. May contain as many as NBMAX_DISKS (maximum
   number of disks in a cluster). */
typedef struct broken_disk_table broken_disk_table_t;

/**
 * Load a broken disk table.
 *
 * @param[out] table            Loaded broken disk table
 * @param[in]  filename         Broken disk table file
 * @param[in]  open_read_write  Whether we want to write to the table.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int broken_disk_table_load(broken_disk_table_t **table, const char *filename,
                           bool open_read_write);

/**
 * Unload a broken disk table.
 *
 * The table is set to NULL upon return.
 *
 * @param[in,out] table  Broken disk table
 */
void broken_disk_table_unload(broken_disk_table_t **table);

/**
 * Write the broken disk table to disk.
 *
 * (Synchronizes the on-disk version of the table with the in-memory
 * version.)
 *
 * @param[in] table  Table to write
 *
 * @return 0 if successful, a negative error code otherwise
 */
int broken_disk_table_write(broken_disk_table_t *table);

/**
 * Set the disk at a specified index in a broken disk table.
 *
 * @param[in,out] table      Broken disk table
 * @param[in]     index      Index of disk to set
 * @param[in]     disk_uuid  UUID of disk
 *
 * @return 0 if successful, a negative error code otherwise
 */
int broken_disk_table_set_disk(broken_disk_table_t *table, int index,
                               const exa_uuid_t *disk_uuid);

/**
 * Get the disk at a specified index in a broken disk table.
 *
 * @param[in] table  Broken disk table
 * @param[in] index  Index of disk to get
 *
 * @return UUID of broken disk or NULL if there's no disk at index
 *         (or index is out of range)
 */
const exa_uuid_t *broken_disk_table_get_disk(const broken_disk_table_t *table,
                                        int index);

/**
 * Set the array of broken disks of a broken disk table.
 *
 * The on-disk version of the table is updated as well.
 *
 * @param[in,out] table  Broken disk table (already opened)
 * @param[in]     uuids  Array of NBMAX_DISKS disk UUIDs
 *
 * @return 0 if successful, a negative error code otherwise
 */
int broken_disk_table_set(broken_disk_table_t *table, const exa_uuid_t *uuids);

/**
 * Get the array of broken disks from a broken disk table.
 *
 * @param[in] table  Broken disk table
 *
 * @return Raw array of NBMAX_DISKS disks UUIDs
 */
const exa_uuid_t *broken_disk_table_get(const broken_disk_table_t *table);

/**
 * Tell whether a broken disk table contains a disk.
 *
 * @param[in] table      Broken disk table
 * @param[in] disk_uuid  UUID of disk to check
 *
 * @return true if the table contains the disk, false otherwise
 *         (also returns false if either of table and disk_uuid is NULL)
 */
bool broken_disk_table_contains(const broken_disk_table_t *table,
                                const exa_uuid_t *disk_uuid);

/**
 * Clear a broken disk table.
 *
 * Only clears the in-memory version of the table; the on-disk version
 * is not updated.
 *
 * @param[in,out] table  Broken disk table
 *
 * @return 0 if successful, a negative error code otherwise
 */
int broken_disk_table_clear(broken_disk_table_t *table);

/**
 * Get a broken disks table's version.
 *
 * @param[in] table     Broken disk table
 *
 * @return the table's version
 */
uint64_t broken_disk_table_get_version(const broken_disk_table_t *table);

/**
 * Set a broken disks table's version.
 *
 * @param[in] table     Broken disk table
 * @param[in] version   The new version
 *
 * @return 0 if successful, a negative error code otherwise
 *
 */
int broken_disk_table_set_version(broken_disk_table_t *table,
                                       uint64_t version);

/**
 * Increment a broken disks table's version.
 *
 * @param[in] table     Broken disk table
 *
 * @return 0 if successful, a negative error code otherwise
 *
 */
int broken_disk_table_increment_version(broken_disk_table_t *table);

#endif /* __BROKEN_DISK_TABLE_H */
