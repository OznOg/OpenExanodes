/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_METADATA_BLOCK_H__
#define __RAIN1_METADATA_BLOCK_H__

#include "os/include/os_thread.h"

#include "common/include/exa_constants.h"

#include "vrt/common/include/list.h"

#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"

/**
 * Metadata stored for each dirty zone.
 *
 * FIXME: Currently, we do not take care of endianess while handling
 * metadata. As we don't want to swap bytes every time we need to
 * write the metadata (performance critical path), it might be a good
 * idea to store the endianess of each node along with the metadata
 * themselves, so that when a node needs to read other's node
 * metadata, it will know how to handle the endianess.
 */
typedef struct
{
    sync_tag_t sync_tag;
    uint16_t   write_pending_counter;
} desync_info_t;

/**
 * Size of a metadata block (in bytes).
 */
#define METADATA_BLOCK_SIZE SECTOR_SIZE

/**
 * Number of dirty zones represented in each metadata block.
 */
#define DZONE_PER_METADATA_BLOCK (METADATA_BLOCK_SIZE / sizeof(desync_info_t))

/**
 * Internal structure representing a 4 Kb block of metadata. It is used to do
 * the necessary locking while metadata are being written to the disk.
 */
typedef struct
{
    /** A lock under which metadata of the current block have to
        be modified (both the modification of the metadata
        themselves, and the metadata block status below) */
    os_thread_mutex_t lock;

    /** Is the block being flushed to disk */
    bool ongoing_flush;

    /** Does the metadata block need to be flushed to disk */
    bool flush_needed;

    /** "Live" version of the dirty zone metadata */
    desync_info_t in_memory_metadata[DZONE_PER_METADATA_BLOCK];

    /** Copy of the dirty zone metadata stored on disk */
    desync_info_t on_disk_metadata[DZONE_PER_METADATA_BLOCK];

    /** List of the requests that are waiting for this metadata
        block to become available for update and writing */
    struct list_head wait_avail_list;

    /** List of the requests that have updated the metadata block
        in-memory and that are waiting for the completion of the
        write to go further */
    struct list_head wait_write_list;

} slot_desync_info_t;


/**
 * Test if the desync information is valid
 *
 * @param[in] desync_info       the desync info to test
 * @param[in] current_sync_tag  the current synchronization tag
 *
 * @return true or false
 */
bool desync_info_is_valid(const desync_info_t *desync_info,
                          sync_tag_t current_sync_tag);

void dzone_metadata_block_merge(desync_info_t *merged_metadatas,
                                const desync_info_t *metadatas);

void dzone_metadata_block_init(desync_info_t *metadatas,
                               sync_tag_t uptodate_tag);

/**
 * Allocate memory to store information about each block of on-disk metadata.
 * Metadata on disks are stored on blocks of 1 sector * that cannot be written
 * in parallel. So, we need some synchronization to avoid this, and the
 * "slot_desync_info_t" contains the needed informations.
 * the block represents a 4 Kb block of metadata. It is used to do the
 * necessary locking while metadata are being written to the disk.
 *
 * @param[in] sync_tag the sync tag for this slot
 *
 * return a valid slot_desync_info_t *
 */
slot_desync_info_t *slot_desync_info_alloc(sync_tag_t sync_tag);

/**
 * Delete a slot_desync_info_t previously allocated with slot_desync_info_alloc
 *
 * @param[in] block the block of metadata to be freed.
 */
#define slot_desync_info_free(block) (__slot_desync_info_free(block), (block) = NULL)
void __slot_desync_info_free(slot_desync_info_t *block);

#endif

