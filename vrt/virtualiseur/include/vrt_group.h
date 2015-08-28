/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_GROUP_H__
#define __VRT_GROUP_H__

/* XXX Should include vrt_layout.h, but it fails to compile because
       vrt_layout.h uses vrt_group_t... */

#include "vrt/virtualiseur/include/storage.h"
#include "vrt/virtualiseur/include/vrt_common.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"

#include "vrt/common/include/list.h"
#include "vrt/common/include/vrt_stream.h"
#include "vrt/common/include/waitqueue.h"

#include "common/include/uuid.h"
#include "common/include/exa_constants.h"

#include "os/include/os_thread.h"
#include "os/include/os_atomic.h"
#include "os/include/os_inttypes.h"

/* FIXME This header is a mess. Functions are in anything but some rational
         ordering and the worse is, the .c is just as messy. */

/** Data structure used to represent a group */
typedef struct vrt_group
{
    /** Chaining for the global list of all groups */
    struct list_head list;

    /** The UUID that uniquely identifies the group */
    exa_uuid_t uuid;

    /** The name */
    char name[EXA_MAXSIZE_GROUPNAME + 1];

    /** Status of the group */
    exa_group_status_t status;

    /** Usage count */
    os_atomic_t count;

    /* Information about the group's underlying storage */
    storage_t *storage;

    struct {
        os_thread_t tid; /**< thread id */
        bool run; /**< Tells the rebuild thread to start */
        bool ask_terminate;
        os_sem_t sem; /**< Semaphore to wake up the rebuild thread */

        os_atomic_t running;
        wait_queue_head_t wq;
    } rebuild_thread; /**< rebuild_thread information */

    struct {
        os_thread_t tid; /**< thread id */
        bool run; /**< Tells the metadata thread to start */
        os_sem_t sem; /**< Semaphore to wake up the metadata thread */
        bool ask_terminate; /**< to terminate metadata thread */

        os_atomic_t running;
        wait_queue_head_t wq;
    } metadata_thread;

    /** Set during a recovery */
    int suspended;

    /** Protect suspend status. Taking this lock makes sure we won't
     *  get (un)suspended in the middle of what we're doing.
     *  FIXME this is guessed from the previous giant group->lock */
    os_thread_rwlock_t suspend_lock;

    /** Number of volumes created in this group */
    uint32_t nb_volumes;

    /** The volumes stored in this group */
    struct vrt_volume *volumes[NBMAX_VOLUMES_PER_GROUP];

    /** The layout used to place data in this group */
    const struct vrt_layout *layout;

    /** Group data that are specific to the layout */
    void *layout_data;

    /** Waitqueue on which processes waiting to access the group are
	registered (when the group is suspended) */
    wait_queue_head_t suspended_req_wq;

    /** Waitqueue on which the recover thread will wait until all
	"initialized requests" are completed (until initialized_request_count,
	defined below, becomes 0). */
    wait_queue_head_t recover_wq;

    /** The number of request handled by the virtualizer that are initialized.
	It is used to be able to wait until all requests layers have been
	reinitialized. */
    os_atomic_t initialized_request_count;

    /** Protect build_io_for_req() against changes of group and realdev status,
     * either during the recovery of in reintegrate */
    os_thread_rwlock_t status_lock;

    /** Version number that the superblocks should have */
    uint64_t sb_version;
} vrt_group_t;

/**
 * Internal structures describing a group's information (group, rdevs and
 * volumes). They're used to  build up the necessary information from admind
 * before being able to create the group cleanly.
 */

 /** Structure describing layout settings */
typedef struct
{
    bool is_set;                            /** Whether the following info is available */
    uint32_t slot_width;                    /** Slot width */
    uint32_t chunk_size;                    /** Chunk size in sectors */
    uint32_t su_size;                       /** Striping unit size in sectors */
    uint32_t dirty_zone_size;               /** Dirty zone size in sectors */
    bool blended_stripes;                   /** Are stripes blended or not */
    uint32_t nb_spares;                      /** Number of spares */
} vrt_group_layout_info_t;

/** Structure describing a group */
typedef struct
{
    char name[EXA_MAXSIZE_GROUPNAME + 1];                   /** Group's name */
    exa_uuid_t uuid;                                        /** Group's UUID */
    char layout_name[EXA_MAXSIZE_LAYOUTNAME + 1];           /** Group's layout name */
    uint64_t sb_version;                                    /** Group's SB version */

    uint32_t nb_rdevs;                                      /** Number of rdevs */
    vrt_rdev_info_t rdevs[NBMAX_DISKS_PER_GROUP + 1];       /** List of rdevs */

    vrt_group_layout_info_t layout_info;                    /** Layout settings */
} vrt_group_info_t;

void vrt_group_info_init(vrt_group_info_t *info);

vrt_group_t *vrt_group_alloc(const char *group_name, const exa_uuid_t *group_uuid,
                             const struct vrt_layout *layout);
void __vrt_group_free(struct vrt_group *group);
#define vrt_group_free(group) (__vrt_group_free(group), group = NULL)

int vrt_group_build_from_description(vrt_group_t **group,
                                     const vrt_group_info_t *group_desc,
                                     char *error_msg);

int vrt_group_create(const vrt_group_info_t *group_description,
                     char *error_msg);

int vrt_group_start(const vrt_group_info_t *group_description,
                    vrt_group_t **started_group);

int vrt_group_insert_rdev(const vrt_group_info_t *group_description,
                          const exa_uuid_t *uuid, const exa_uuid_t *nbd_uuid,
                          exa_nodeid_t node_id, spof_id_t spof_id, bool local,
                          uint64_t old_sb_version, uint64_t new_sb_version);

int vrt_group_stop(vrt_group_t *group);
int vrt_group_stoppable(vrt_group_t *group, const exa_uuid_t *group_uuid);

bool vrt_group_ref(struct vrt_group *group);
void vrt_group_unref(struct vrt_group *group);

/**
 * Find a volume in a group using its UUID.
 *
 * @param[in] group  Group in which the volume has to be searched
 * @param[in] uuid   UUID of the volume to look for
 *
 * @return volume if found, NULL otherwise
 */
struct vrt_volume *vrt_group_find_volume(const struct vrt_group *group,
                                         const exa_uuid_t *uuid);

/**
 * Create a new volume in a group.
 *
 * @param[in] group     Group in which to create the volume
 * @param[out] volume   Pointer to the new volume
 * @param[in] uuid      UUID of the new volume
 * @param[in] name      Name of the new volume
 * @param[in] size      Size of the new volume in sectors (must be non-zero)
 *
 * @return EXA_SUCCESS if the volume was successfully created, a negative
 *         error code otherwise
 */
int vrt_group_create_volume(vrt_group_t *group, vrt_volume_t **volume,
                            const exa_uuid_t *uuid, const char *name,
                            uint64_t size);

/**
 * Wipe a volume.
 *
 * @param[in] group     The group of the volume
 * @param[in] volume    The volume to wipe
 *
 * @return 0 if successful, a negative error code otherwise.
 *
 * @attention Swallow I/O errors in case the group is offline: this allows
 * to keep the current "best effort" behaviour in creating a volume.
 * However, this is UNDESIRABLE as it leads to an inconsistency: a
 * volume is wiped when created when the group is online, while it is
 * *not* wiped when the group is offline. Moreover, it would be simpler
 * to just refuse to create a volume when a group is offline.
 * See bug #4525.
 */
int vrt_group_wipe_volume(vrt_group_t *group, vrt_volume_t *volume);

int vrt_group_delete_volume(struct vrt_group *group, struct vrt_volume *volume);

int vrt_group_sync_sb(vrt_group_t *group, uint64_t old_sb_version,
                      uint64_t new_sb_version);

int vrt_group_going_offline(vrt_group_t *group, const exa_nodeset_t *stop_nodes);

int vrt_group_resume(vrt_group_t *group);
int vrt_group_suspend(vrt_group_t *group);

int vrt_group_compute_status(vrt_group_t *group);

int vrt_group_resync(vrt_group_t *group, const exa_nodeset_t *nodes);
int vrt_group_post_resync(vrt_group_t *group);

/**
 * Reset the data of a group by writing the same value for the
 * replicas. The typical use for this function is to combine it with
 * vrt_group_check() in order to check data integrity. The sequence is
 * the following:
 * - invoke vrt_group_reset() in order to set the replicas to the same
 *   value
 * - write some data, crash some nodes or disks, rebuild, replicate...
 * - invoke vrt_group_check() to ensure that the replicas are still
 * synchronized.
 * This operation is only supported by the layouts that provide data
 * mirroring.
 *
 * @param[in] group Group whose data must be resetted
 *
 * @return EXA_SUCCESS or a negative error code
 */
int vrt_group_reset(vrt_group_t *group);

/**
 * Check that all the replicas of a group are synchronized.
 * @see vrt_group_reset()
 *
 * @param[in] group
 *
 * @return EXA_SUCCESS or a negative error code
 */
int vrt_group_check(vrt_group_t *group);

/**
 * Tell whether a group supports the replacement of its devices.
 *
 * @param[in] group  Group to check
 *
 * @return true if the group supports replacement, false otherwise
 */
bool vrt_group_supports_device_replacement(const vrt_group_t *group);

/**
 * Reintegrate a device in a group.
 *
 * There are two types of reintegrate:
 *
 *   - Partial reintegrates does not mean that the rebuilding has been
 *     completed: they are used to mark on disk the progression of the
 *     rebuilding.
 *
 *   - Final reintegrates are issued when the rebuilding is completed.
 *
 * @param[in] group                      Group in which to reintegrate the device
 * @param[in] rdev                       Device to reintegrate
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int vrt_group_reintegrate_rdev(vrt_group_t *group, struct vrt_realdev *rdev);

/**
 * Post-reintegrate a device in a group.
 *
 * @param[in] group  Group in which to post-reintegrate the device
 * @param[in] rdev   Device to post-reintegrate
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int vrt_group_post_reintegrate_rdev(vrt_group_t *group,
                                    struct vrt_realdev *rdev);

int vrt_group_rdev_up(vrt_group_t *group, vrt_realdev_t *rdev);
int vrt_group_rdev_down(vrt_group_t *group, vrt_realdev_t *rdev);
int vrt_group_rdev_replace(vrt_group_t *group, vrt_realdev_t *rdev, const exa_uuid_t *new_rdev_uuid);

/* Size in bytes */
uint64_t vrt_group_total_capacity(const vrt_group_t *group);

/* Size in bytes */
uint64_t vrt_group_used_capacity(const vrt_group_t *group);

/**
 * Wait until all requests for the given group have been reinitialized
 * This is called at the beginning of the recovery process, to make sure
 * nothing is happening during the recovery.
 *
 * @param[in] group The group
 */
void vrt_group_wait_initialized_requests(struct vrt_group *group);

typedef enum { GROUP_HEADER_MAGIC = 0x7700FF22 } group_header_magic_t;

#define GROUP_HEADER_FORMAT  1

typedef struct
{
    /* IMPORTANT - Fields 'magic' and 'format' *MUST* always be 1st and 2nd */
    group_header_magic_t magic;
    uint32_t format;
    uint32_t reserved;
    exa_uuid_t uuid;
    char name[EXA_MAXSIZE_GROUPNAME + 1];
    uint32_t nb_volumes;
    char layout_name[EXA_MAXSIZE_LAYOUTNAME + 1];
    uint64_t data_size;
} group_header_t;

int vrt_group_header_read(group_header_t *header, stream_t *stream);

uint64_t vrt_group_serialized_size(const vrt_group_t *group);
int vrt_group_serialize(const vrt_group_t *group, stream_t *stream);
int vrt_group_deserialize(vrt_group_t **group, storage_t *storage,
                             stream_t *stream, const exa_uuid_t *group_uuid);

bool vrt_group_equals(const vrt_group_t *group1, const vrt_group_t *group2);

#endif /* __VRT_GROUP_H__ */
