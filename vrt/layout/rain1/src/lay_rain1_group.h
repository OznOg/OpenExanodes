/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LAYOUT_RAIN1_GROUP_H__
#define __LAYOUT_RAIN1_GROUP_H__


#include "common/include/exa_constants.h"
#include "common/include/exa_math.h"
#include "common/include/uuid.h"

#include "vrt/common/include/waitqueue.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/assembly/src/assembly_volume.h"

#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"

#include "vrt/assembly/src/assembly_group.h"

/**
 * Size of a metadata block (in bytes).
 */
#define METADATA_BLOCK_SIZE SECTOR_SIZE

/**
 * Structure containing the layout-specific information stored in
 * memory for each group.
 */
typedef struct
{
    struct assembly_group assembly_group;

    /** Does the placement uses blended stripes */
    uint32_t blended_stripes;

    /** Size of a striping unit (in sectors) */
    uint32_t su_size;

    /** Max sectors writable with one I/O */
    uint32_t max_sectors;

    /** Synchronization tag corresponding to the one of the up-to-date devices
     * of the group */
    sync_tag_t sync_tag;

    /** Logical slot size. RainX uses replication and supports sparing.
     *  @f[
     *  logical\_slot\_size = (slot\_width) \times chunk\_size
     *                        \times \frac{1}{2}
     *  @f]
     *  The logical slot size is stored in this structure in order to
     *  avoid to perform the computation every time it is used.
     */
    uint64_t logical_slot_size;

    /** Size of a dirty zone in sectors */
    uint32_t dirty_zone_size;

    /** Number of realdevs */
    uint32_t nb_rain1_rdevs;

    /** Array of rdev-related data pointers */
    struct rain1_realdev *rain1_rdevs[NBMAX_DISKS_PER_GROUP];

    /** Used to protect changes in sparing configuration, and status
     *  FIXME: It's still not very well defined. */
    os_thread_rwlock_t status_lock;

    /* In resync, we do not read all metadata but only metadata of relevent nodes
     * for the resync. nodes_resync is the set of nodes relevent for the resync. */
    exa_nodeset_t nodes_resync;

    /* In rebuild-update, we do not read all metadata but only metadata of
     * relevent nodes for the rebuild-update. nodes_update is the set of nodes
     * relevent for the rebuild-update. */
    exa_nodeset_t nodes_update;

    /* pre-allocated structs and pages for resync and rebuild */
    /* FIXME: using struct to avoid including lay_rain1_sync_job that includes
     * rdev that include group ...  This problem will be solved when struct
     * dzone_sync_job will be declared only in the source file.
     */
    struct sync_job_pool *sync_job_pool;
} rain1_group_t;

#define foreach_rainx_rdev(rxg, lr, i)          \
    for ((lr) = (rxg)->rain1_rdevs[0], (i) = 0; \
         (i) < (rxg)->nb_rain1_rdevs;           \
         ++(i), (i) < (rxg)->nb_rain1_rdevs ? (lr) = (rxg)->rain1_rdevs[(i)] : NULL)

#define RAIN1_GROUP(group) ((rain1_group_t *)(group)->layout_data)

rain1_group_t *rain1_group_alloc(void);

void __rain1_group_free(rain1_group_t *rxg, const storage_t *storage);
#define rain1_group_free(rxg, storage) \
    (__rain1_group_free((rxg), (storage)), (rxg) = NULL)

int rain1_create_subspace(rain1_group_t *rxg, const exa_uuid_t *uuid, uint64_t size,
                          struct assembly_volume **av, storage_t *storage);
void rain1_delete_subspace(rain1_group_t *rxg, struct assembly_volume **av,
                           const storage_t *storage);
int rain1_resize_subspace(rain1_group_t *rxg, assembly_volume_t *av,
                          uint64_t new_nb_slots, const storage_t *storage);

bool rain1_group_is_rebuilding(const void *layout_data);

/**
 * Find out if a slot is used, either by a volume or by metadata slots
 *
 * @param[in] lg            The layout group metadata
 * @param[in] slot_index    The slot index
 *
 * @return true if the slot is used
 */
bool rain1_group_is_slot_used(const rain1_group_t *rxg,
                              unsigned int slot_index);

const slot_t *rain1_group_get_slot_by_index(const rain1_group_t *rxg,
                                            unsigned int slot_index);
/**
 * Give logical size that is usable for dirty zone metadata on each slot of the
 * group
 *
 * @param[in] rxg  The rain1 group
 *
 * @return the size in sectors (multiple of su_size)
 */
static inline uint64_t rain1_group_get_slot_metadata_size(const rain1_group_t *rxg)
{
    uint64_t needed_size = EXA_MAX_NODES_NUMBER * BYTES_TO_SECTORS(METADATA_BLOCK_SIZE);

    /* The size we really need is one block of metadata (corresponding to one
     * slot) per node but we must also ensure that both the metadata_size and
     * the data size are multiples of the striping unit size.
     */
    return quotient_ceil64(needed_size, rxg->su_size) * rxg->su_size;
}

/**
 * Give logical size that is usable for data on each slot of the group
 *
 * @param[in] rxg  The rain1 group
 *
 * @return the size in sectors (multiple of su_size)
 */
static inline uint64_t rain1_group_get_slot_data_size(const rain1_group_t *rxg)
{
    uint64_t slot_metadata_size = rain1_group_get_slot_metadata_size(rxg);

    EXA_ASSERT(rxg->logical_slot_size > slot_metadata_size);

    return rxg->logical_slot_size - slot_metadata_size;
}

/**
 * Give the total number of dirty zones in the group
 */
uint64_t rain1_group_get_dzone_per_slot_count(const rain1_group_t *rxg);

unsigned int rain1_group_get_sync_job_blksize(const rain1_group_t *rxg);
unsigned int rain1_group_get_sync_jobs_count(const rain1_group_t *rxg);

bool rain1_group_equals(const rain1_group_t *rxg1,
                        const rain1_group_t *rxg2);
#endif
