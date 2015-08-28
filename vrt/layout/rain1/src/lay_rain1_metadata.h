/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_METADATA_H__
#define __RAIN1_METADATA_H__

#include "vrt/virtualiseur/include/vrt_group.h"

#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_desync_info.h"

/**
 * Write a metadata block corresponding to a slot and a node on the logical
 * space.
 *
 * When the group is rebuilding, IO must be performed in the right order:
 * uptodate replica first, and then non-uptodate replica. Indeed, this function
 * can be used by flush and reset_dirty concurrently with metadata request
 * processing.
 *
 * @param[in] rxg            The layout data
 * @param[in] node_index     The index of the node the metadata belong to
 * @param[in] slot_index     The index of the slot the metadata correspond to
 * @param[in] metadatas      The metadata to write.
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int rain1_write_slot_metadata(const rain1_group_t *rxg,
                              const slot_t *slot,
                              unsigned int node_index,
                              const desync_info_t *metadatas);

/**
 * Read a metadata block corresponding to a slot and a node from the logical
 * space.
 *
 * @param[in] rxg            The layout data
 * @param[in] node_index     The index of the node the metadata belong to
 * @param[in] slot_index     The index of the slot the metadata correspond to
 * @param[out] metadatas     The metadata block to write to.
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int rain1_read_slot_metadata(const rain1_group_t *rxg,
                             const slot_t *slot,
                             unsigned int node_id,
                             desync_info_t *metadatas);

/**
 * Wipe the metadata block corresponding to a slot and all nodes on the logical
 * space.
 *
 * @param[in] rxg            The layout data
 * @param[in] slot
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int rain1_wipe_slot_metadata(const rain1_group_t *rxg, const slot_t *slot);

int rain1_group_metadata_flush_step(void *private_data, void *context,
                                    bool *more_work);
void *rain1_group_metadata_flush_context_alloc(void *layout_data);
void rain1_group_metadata_flush_context_free(void *context);
void rain1_group_metadata_flush_context_reset(void *context);

typedef struct
{
    exa_uuid_t current_subspace_uuid;
    uint64_t current_slot_index;
} rain1_metadata_flush_context;
#endif
