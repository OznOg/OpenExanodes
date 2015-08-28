/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SPOF_GROUP_H__
#define __SPOF_GROUP_H__

#include "vrt/virtualiseur/include/chunk.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"

#include "os/include/os_inttypes.h"

/** Structure describing a SPOF group
 * FIXME: rename
 */
typedef struct
{
    /** The SPOF id */
    spof_id_t spof_id;

    /** Array of disks that are part of the SPOF group */
    struct vrt_realdev *realdevs[NBMAX_DISKS_PER_SPOF_GROUP];

    /** Number of elements in the previous array */
    uint32_t nb_realdevs;
} spof_group_t;

/**
 * Initialize a spof group.
 *
 * The spof group's id is set to SPOF_ID_NONE and the spof group is
 * empty (no realdevs in it).
 *
 * @param[out] spof_group  Spof group to initialize
 */
void spof_group_init(spof_group_t *spof_group);

/**
 * Cleanup a spof group.
 *
 * The spof group's id is reset to SPOD_ID_NONE and the spof group
 * is emptied (all realdevs are removed from it).
 *
 * @param[in,out] spof_group  Spof group to clean up
 */
void spof_group_cleanup(spof_group_t *spof_group);

/**
 * Add an rdev to a spof group.
 *
 * @param[in,out] spof_group  Spof group to add to
 * @param[in]     rdev        Rdev to add
 *
 * @return 0 if successful, -ENOSPC if spof group is full (already contains
 *           the maximum number of rdevs allowed)
 */
int spof_group_add_rdev(spof_group_t *spof_group, vrt_realdev_t *rdev);

/**
 * Remove an rdev from a spof group.
 *
 * @param[in,out] spof_group  Spof group to remove from
 * @param[in]     rdev        Rdev to remove
 *
 * @return 0 if successful, -ENOENT if the rdev wasn't found.
 */
int spof_group_del_rdev(spof_group_t *spof_group, const vrt_realdev_t *rdev);

/**
 * Returns all nodes participating in a spof group
 *
 * @param[in]   spof_group  Spof group to query
 * @param[out]  nodes       A nodeset describing nodes in the spof group
 */
void spof_group_get_nodes(const spof_group_t *spof_group, exa_nodeset_t *nodes);

/**
 * Looks for a given ID in a spof group table, and return the corresponding
 * spof group.
 *
 * @param[in]  id           The ID to search
 * @param[in]  spofs        The table of spof groups
 * @param[in]  num_spofs    The number of groups in the table
 *
 * @return the SPOF group with the given ID, or NULL if it wasn't found
 */
spof_group_t *spof_group_lookup(spof_id_t id, spof_group_t spofs[],
                                      uint32_t num_spofs);
/**
 * Creates a new chunk from a given spof. The chunk is created from the
 * rdev of the spof that has the largest number of free chunks.
 *
 * @param[in] spof    The spof in which we want to take the new chunk
 *
 * @return an allocated and initialized chunk
 */
chunk_t *spof_group_get_chunk(spof_group_t *spof_group);

/**
 * Destroys a chunk.
 *
 * @param[in] chunk    The chunk
 */
void __spof_group_put_chunk(chunk_t *chunk);
#define spof_group_put_chunk(c) ( __spof_group_put_chunk(c), (c) = NULL )

uint32_t spof_group_free_chunk_count(const spof_group_t *spof_group);

/**
 * @brief Return the total number of chunks in a SPOF group
 *
 * @param[in] spof_group SPOF group
 *
 * @return The total number of chunks
 */
uint64_t spof_group_total_chunk_count(const spof_group_t *spof_group);

#endif  /* __SPOF_GROUP_H__ */
