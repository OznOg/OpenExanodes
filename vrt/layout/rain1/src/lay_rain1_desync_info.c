/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/layout/rain1/src/lay_rain1_desync_info.h"

#include "vrt/virtualiseur/include/vrt_request.h"
#include "common/include/exa_math.h"

#include "os/include/os_mem.h"

void dzone_metadata_block_init(desync_info_t *metadatas,
                               sync_tag_t uptodate_tag)
{
    unsigned int i;
    EXA_ASSERT(metadatas != NULL);

    for (i = 0; i < DZONE_PER_METADATA_BLOCK; i++)
    {
        metadatas[i].write_pending_counter = 0;
        metadatas[i].sync_tag = uptodate_tag;
    }
}

void dzone_metadata_block_merge(desync_info_t *merged_metadatas,
                                const desync_info_t *metadatas)
{
    unsigned int i;
    EXA_ASSERT(metadatas != NULL);

    for (i = 0; i < DZONE_PER_METADATA_BLOCK; i++)
    {
        merged_metadatas[i].write_pending_counter =
            MAX(merged_metadatas[i].write_pending_counter,
                metadatas[i].write_pending_counter);
        merged_metadatas[i].sync_tag = sync_tag_max(merged_metadatas[i].sync_tag,
                                                    metadatas[i].sync_tag);
    }
}

bool desync_info_is_valid(const desync_info_t *desync_info,
                          sync_tag_t current_sync_tag)
{
    return desync_info->write_pending_counter <= vrt_get_max_requests()
        && sync_tags_are_comparable(desync_info->sync_tag, current_sync_tag)
        && !sync_tag_is_greater(desync_info->sync_tag, current_sync_tag);
}

slot_desync_info_t *slot_desync_info_alloc(sync_tag_t sync_tag)
{
    slot_desync_info_t *block = os_malloc(sizeof(slot_desync_info_t));

    EXA_ASSERT_VERBOSE(block != NULL, "Failed to allocate memory for "
                       "desynchonization data");

    os_thread_mutex_init(&block->lock);

    block->ongoing_flush = false;
    block->flush_needed = false;

    INIT_LIST_HEAD(&block->wait_avail_list);
    INIT_LIST_HEAD(&block->wait_write_list);

    /* FIXME: I think that the meta-data should be read from the disk and
     * not initialized.
     * At least it would be simpler to aprehend the side effects... */
    dzone_metadata_block_init(block->in_memory_metadata, sync_tag);
    dzone_metadata_block_init(block->on_disk_metadata,   sync_tag);

    return block;
}

void __slot_desync_info_free(slot_desync_info_t *block)
{
    if (block == NULL)
        return;

    os_thread_mutex_destroy(&block->lock);
    os_free(block);
}
