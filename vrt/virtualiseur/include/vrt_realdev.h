/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_REALDEV_H__
#define __VRT_REALDEV_H__

#include "vrt/virtualiseur/include/realdev_superblock.h"
#include "vrt/virtualiseur/include/vrt_common.h"

#include "vrt/common/include/spof.h"
#include "vrt/common/include/vrt_stream.h"

#include "vrt/assembly/include/extent.h"

#include "blockdevice/include/blockdevice.h"

#include "common/include/uuid.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"

#include <sys/types.h>

/** Size of VRT's superblock area (for both "positions" 0 and 1 of the
   superblock), in sectors */
#define VRT_SB_AREA_SIZE  65536  /* 32 MiB */

/**
 * This structure describes a real device handled by Exanodes.
 * It is disk accessed through the NBD.
 */
typedef struct vrt_realdev
{
    /** UUID of the device in the VRT */
    exa_uuid_t  uuid;

    /** Assembly information (chunk splitting, chunk usage and so on) */
    struct
    {
        uint32_t chunk_size;          /**< Chunk size in sectors */
        uint32_t total_chunks_count;  /**< Total number of chunks in the device */
        uint32_t free_chunks_count;   /**< Number of free (available) chunks */
        extent_t *free_chunks;        /**< List of free (available) chunks extents */
    } chunks;

    /** UUID of the device in the NBD */
    exa_uuid_t  nbd_uuid;

    /** The node on which the rdev is. */
    exa_nodeid_t node_id;

    /** The SPOF on which the rdev is. */
    spof_id_t spof_id;

    /** Physical size in sectors */
    uint64_t real_size;

    /** True if the rdev is local  */
    bool local;

    /** Block device (NBD) */
    blockdevice_t *blockdevice;

    /** Real device primitive status : is the device UP */
    exa_bool_t up;

    /** Real device primitive status : is the device superblock corrupted */
    exa_bool_t corrupted;

    /** Flat index of the rdev in the storage */
    int index;

    /* Stream plumbing schema
     *
     *    checksum_sb_streams[0] -> sb_data_streams[0] \
     *                                                  }-> raw_sb_stream
     *    checksum_sb_streams[1] -> sb_data_streams[1] /
     */

    /** The raw stream to write all the superblock information on the rdev */
    stream_t *raw_sb_stream;

    /** The streams corresponding to the two versions of superblocks we need to
     * maintain on each rdev */
    stream_t *sb_data_streams[2];

    /** The streams that compute the checksums for the superblocks  */
    stream_t *checksum_sb_streams[2];
} vrt_realdev_t;

/** Internal flat structure describing an rdev */
typedef struct
{
    exa_uuid_t uuid;        /** The rdev's VRT uuid */
    exa_uuid_t nbd_uuid;    /** The rdev's NBD uuid */
    exa_nodeid_t node_id;   /** The node id of the node in which the rdev is */

    bool local;             /** Whether the rdev is local */
    bool up;                /** Whether the rdev is up */

    spof_id_t spof_id;      /** The rdev's SPOF group's id */
} vrt_rdev_info_t;

/* FIXME Quite artificial - it was initially using a structure independent
         from rdev (assembly_device_info_t). Should probably get rid of it. */
uint64_t rdev_chunk_based_size(const vrt_realdev_t *rdev);

/**
 * Get the 'compound' status from the primitive status information.
 *
 * @param[in] rdev The real device
 *
 * @return the compound status of the device.
 */
exa_realdev_status_t rdev_get_compound_status(const struct vrt_realdev *rdev);


/**
 * Tell whether the real device is local or not.
 *
 * @param[in] rdev The real device
 *
 * @return true if the real device is local or
 *         false if the real device is remote.
 */
static inline bool rdev_is_local(const struct vrt_realdev *rdev)
{
    return rdev->local;
}

/**
 * Tells whether the real device is UP.
 *
 * @param[in] rdev The real device
 *
 * @return TRUE if the real device is UP or FALSE if not.
 */
static inline exa_bool_t rdev_is_up(const struct vrt_realdev *rdev)
{
    return rdev->up;
}


/**
 * Tells whether the real device is corrupted.
 *
 * @param[in] rdev The real device
 *
 * @return TRUE if the real device belongs to exanodes or FALSE if not.
 */
static inline exa_bool_t rdev_is_corrupted(const struct vrt_realdev *rdev)
{
    return rdev->corrupted;
}


/**
 * Tells whether the real device is OK (up and not corrupted)
 *
 * @param[in] rdev The real device
 *
 * @return TRUE if the real device is UP and not corrupted.
 */
static inline exa_bool_t rdev_is_ok(const struct vrt_realdev *rdev)
{
    return rdev->up && !rdev->corrupted;
}

int vrt_rdev_lock_sectors(struct vrt_realdev *rdev, unsigned long start,
                          unsigned long end);
int vrt_rdev_unlock_sectors(struct vrt_realdev *rdev, unsigned long start,
                            unsigned long end);

/**
 * Get the ID of the node where the device is attached
 */
exa_nodeid_t vrt_rdev_get_nodeid(const struct vrt_realdev *rdev);

/**
 * Triggers a device reintegration of the devices of ... through chech up
 * mechanism.
 *
 * FIXME: as no group ID or device ID is communicated to admind it seems that
 * admind will trigger the reintegrate on all the devices that 'needs' it or on
 * all the devices.
 *
 * @return EXA_SUCCESS or a negative error code.
 */
int vrt_msg_reintegrate_device(void);

/* vrt_realdev.c */
struct vrt_realdev *vrt_rdev_new(exa_nodeid_t node_id,
                                 spof_id_t spof_id, const exa_uuid_t *uuid,
                                 const exa_uuid_t *nbd_uuid, int index,
                                 bool local, bool up);

uint64_t vrt_realdev_get_usable_size(const vrt_realdev_t *rdev);

void __vrt_rdev_free(struct vrt_realdev *rdev);
#define vrt_rdev_free(rdev) ( __vrt_rdev_free(rdev), rdev = NULL )

/**
 * Open a vrt device
 * The function mainly associate the VRT device with the underlying NBD device
 *
 * @param[inout] rdev   the VRT device to open
 *
 * @return EXA_SUCCESS or a negative error code
 *
 * @note it allocates some memory the rdev streams
 */
int vrt_rdev_open(struct vrt_realdev *rdev);

/**
 * Replace the NBD device associated to a VRT device
 * The function re-open the device
 *
 * @param[inout] rdev            the VRT device to open
 * @param[in]    new_rdev_uuid   UUID of the new NBD device
 *
 * @return EXA_SUCCESS or a negative error code
 *
 * @note it allocates some memory the rdev streams
 */
int vrt_rdev_replace(struct vrt_realdev *rdev, const exa_uuid_t *new_rdev_uuid);

int vrt_rdev_up(struct vrt_realdev *rdev);
void vrt_rdev_down(struct vrt_realdev *rdev);

int vrt_rdev_set_real_size(struct vrt_realdev *rdev, uint64_t size);
/* Superblock */

int vrt_rdev_create_superblocks(vrt_realdev_t *rdev);

typedef struct
{
    superblock_header_t header;
    uint64_t new_sb_version;
    stream_t *stream;
} superblock_write_op_t;

int vrt_rdev_begin_superblock_write(vrt_realdev_t *rdev, uint64_t old_version,
                                    uint64_t new_version, superblock_write_op_t *op);
int vrt_rdev_end_superblock_write(vrt_realdev_t *rdev, superblock_write_op_t *op);

typedef struct
{
    superblock_header_t header;
    uint64_t sb_version;
    stream_t *stream;
} superblock_read_op_t;

int vrt_rdev_begin_superblock_read(vrt_realdev_t *rdev, uint64_t sb_version,
                                   superblock_read_op_t *op);
int vrt_rdev_end_superblock_read(vrt_realdev_t *rdev, superblock_read_op_t *op);

#endif /* __VRT_REALDEV_H__ */
