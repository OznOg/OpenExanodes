/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_RDEV_H__
#define __RAIN1_RDEV_H__

#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"

#include "vrt/virtualiseur/include/vrt_realdev.h"

#include "vrt/common/include/spof.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_thread.h"

typedef struct {
    /** lock protecting reintegrate_info from concurrent accesses by the
     * vrt_recover and vrt rebuilding threads */
    os_thread_mutex_t lock;

    /** complete reintegrate (TRUE) or partial reintegrate (FALSE) */
    exa_bool_t complete;

    uint64_t nb_slots_rebuilt;
} rdev_rebuild_progress_t;

/*
 * FIXME: 'none' is not really a kind of rebuilding it would be better to
 * use an additional boolean to indicate if there is a rebuilding.
 */
typedef enum
{
    EXA_RDEV_REBUILD_NONE,        /**< The device is not rebuilding */
    EXA_RDEV_REBUILD_UPDATING,	  /**< The device is updating */
    EXA_RDEV_REBUILD_REPLICATING  /**< The device is replicating */
} rdev_rebuild_type_t;

typedef struct
{
    /** Type of rebuilding */
    rdev_rebuild_type_t type;

    /** Logical date at which the rebuilding was initiated */
    sync_tag_t sync_tag;
} rdev_rebuild_desc_t;

/**
 * Structure containing the layout-specific information stored in
 * memory for each realdev.
 */
typedef struct rain1_realdev
{
    exa_uuid_t uuid; /** uuid of the rain1 rdev */

    vrt_realdev_t *rdev;

    /** is the local instance owner of this rain1 rdev
     * This is mostly used for rebuilding and resync, each instance performs
     * operations on the rain1 rdev it owns */
    bool mine;

    /** Synchronization tag corresponding to the logical date until which we
     * ensure the device has been kept synchronized */
    sync_tag_t sync_tag;

    /** Description of the on-going rebuilding on this device */
    rdev_rebuild_desc_t rebuild_desc;

    /** Information to track the rebuilding progression */
    rdev_rebuild_progress_t rebuild_progress;
} rain1_realdev_t;

#define RAIN1_REALDEV(rxg, rdev) ((rxg)->rain1_rdevs[(rdev)->index])

/**
 * Initialize the rebulding context of the device
 *
 * @param[out] rain1_realdev   Pointer to the realdev
 * @param[in]  type            The type of rebuilding
 * @param[in]  sync_tag        The logical date at which the rebuilding is
 *                             initated
 */
void rain1_rdev_init_rebuild_context(struct rain1_realdev *lr,
                                     rdev_rebuild_type_t type,
                                     sync_tag_t sync_tag);

/**
 * Clear the rebulding context of the device
 *
 * @param[out] rain1_realdev  Pointer to the realdev
 */
void rain1_rdev_clear_rebuild_context(struct rain1_realdev *lr);

/**
 * Tells whether the real device is up-to-date relatively to a sync tag.
 *
 * @param[in] lr        The real device's layout data
 * @param[in] sync_tag  The sync tag to check against
 *
 * @return TRUE if the real device is up-to-date or FALSE if not.
 */
exa_bool_t rain1_rdev_is_uptodate(const rain1_realdev_t *lr, sync_tag_t sync_tag);

/**
 * Tells whether the real device is updating.
 *
 * @param[in] lr The real device's layout data
 *
 * @return TRUE if the real device is updating or FALSE if not.
 */
exa_bool_t rain1_rdev_is_updating(const struct rain1_realdev *lr);

/**
 * Tells whether the real device is replicating.
 *
 * @param[in] lr The real device's layout data
 *
 * @return TRUE if the real device is replicating or FALSE if not.
 */
exa_bool_t rain1_rdev_is_replicating(const struct rain1_realdev *lr);

/**
 * Predicate: TRUE if the rain1 layout can write on this device. The
 *            rain1 layout can write on a device if it is OK (i.e. UP
 *            and not corrupted) and up-to-date or updating. The rain1
 *            layout MUST NOT write on the device if it is out-of-date
 *            and not updating.
 *
 * @param[in] rxg   the group layout data
 * @param[in] rdev  Device
 *
 * @return TRUE if the layout can write on the device, FALSE otherwise
 */
exa_bool_t rain1_rdev_is_writable(const rain1_group_t *rxg, const struct vrt_realdev *rdev);

/**
 * Tells whether the real device is rebuilding.
 *
 * @param[in] lr The real device's layout data
 *
 * @return TRUE if the real device is rebuilding or FALSE if not.
 */
exa_bool_t rain1_rdev_is_rebuilding(const struct rain1_realdev *lr);

/**
 * Structure containing the information concerning
 * an access point on a given real device.
 */
struct rdev_location
{
    struct vrt_realdev *rdev;	/**< The device owning the location */
    uint64_t sector;		/**< The position in the device */
    unsigned long size;		/**< The size of the location */
    int uptodate;		/**< Is the location up-to-date*/
    int never_replicated;	/**< Is the location in the case 'never replicated' */
};

/**
 * Tells whether a location on a real device needs an update.
 *
 * @param[in] rdev_loc 	The real device location
 *
 * @return TRUE if the location is readable or FALSE if not.
 */
exa_bool_t rain1_rdev_location_readable(const struct rdev_location *rdev_loc);

/**
 * Tells whether a location on a real device needs an update.
 *
 * @param[in] rdev_loc 	The real device location
 *
 * @return TRUE if the location needs an updat or FALSE if not.
 */
exa_bool_t rain1_rdev_location_update_needed(const struct rdev_location *rdev_loc);

struct rain1_realdev *rain1_alloc_rdev_layout_data(vrt_realdev_t *rdev);

void rain1_free_rdev_layout_data(struct rain1_realdev *lr);

bool rain1_realdev_equals(const rain1_realdev_t *lr1, const rain1_realdev_t *lr2);
#endif /* __RAIN1_RDEV_H__ */
