/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * @file
 *
 * This file contains definitions that are used to communicate the state
 * of the virtualizer to admind. It must be includable both in user-space
 * and kernel-space.
 */

#ifndef __VRT_COMMON_H__
#define __VRT_COMMON_H__

#include "common/include/uuid.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"

#include "os/include/os_inttypes.h"

/**
 * Admind status of a VRT resource
 */
typedef enum
{
    VRT_ADMIND_STATUS_DOWN,
    VRT_ADMIND_STATUS_UP
} vrt_admind_status_t;


/**
 * Status of a group
 */
typedef enum
{
    EXA_GROUP_OK,        /**< Fully functional with all rdevs in sync */
    EXA_GROUP_DEGRADED,  /**< Fully functional with some devices down or outdated */
    EXA_GROUP_OFFLINE    /**< Writes always fails, reads may fail */
} exa_group_status_t;


/**
 * Status of a real device:
 */
typedef enum
{
    EXA_REALDEV_OK = 0,		/**< the rdev is in good health 		-- only used for information purpose */
    EXA_REALDEV_DOWN,		/**< the rdev is declared down by admind 	-- only used for information purpose */
    EXA_REALDEV_ALIEN,		/**< the rdev does not belong to Exanodes world	-- only used for information purpose */
    EXA_REALDEV_BLANK,		/**< the rdev is up and blank			-- only used for information purpose */
    EXA_REALDEV_OUTDATED,	/**< the rdev is up but out of sync		-- only used for information purpose */
    EXA_REALDEV_UPDATING,	/**< the rdev is updating 			-- only used for information purpose */
    EXA_REALDEV_REPLICATING	/**< the rdev is replicating another rdev 	-- only used for information purpose */
} exa_realdev_status_t;


/**
 * Status of a volume:
 * - EXA_VOLUME_STOPPED: the volume is stopped,
 * - EXA_VOLUME_STARTED: the volume has been started,
 */
typedef enum {
    EXA_VOLUME_STOPPED,
    EXA_VOLUME_STARTED
} exa_volume_status_t;


typedef enum
{
#define VRT_IO_TYPE__FIRST   VRT_IO_TYPE_READ
    VRT_IO_TYPE_READ = 777,
    VRT_IO_TYPE_WRITE,
    VRT_IO_TYPE_WRITE_BARRIER,
    VRT_IO_TYPE_NONE
#define VRT_IO_TYPE__LAST   VRT_IO_TYPE_NONE
} vrt_io_type_t;

#define VRT_IO_TYPE_IS_VALID(io_type)  \
    ((io_type) >= VRT_IO_TYPE__FIRST && (io_type) <= VRT_IO_TYPE__LAST)

/** Informations about a group */
/* Warning keep aligned on 64 bits */
struct vrt_group_info
{
    /** Group UUID */
    exa_uuid_t uuid;

    /** Group name */
    char name [EXA_MAXSIZE_GROUPNAME+1];

    /** Group status */
    exa_group_status_t status;

    /* True if the group is performing a rebuilding */
    uint32_t is_rebuilding;

    /** Group usable logical capacity (in KB) */
    uint64_t usable_capacity;

    /** Group used logical capacity (in KB) */
    uint64_t used_capacity;

    /** Layout name */
    char layout_name[EXA_MAXSIZE_LAYOUTNAME + 1];

    /** Number of spares, or -1 if not applicable */
    int nb_spare;
    /** Number of spares available, or -1 if not applicable */
    int nb_spare_available;

    /** Number of chunks per slot */
    uint32_t slot_width;

    /** Size of a striping unit (in KB) */
    uint32_t su_size;

    /** Size of a chunk (in KB) */
    uint32_t chunk_size;

    /** Size of a dirty zone (in KB) */
    char dirty_zone_size[MAXLEN_UINT64 + 1];

    /** Does the placement uses blended stripes */
    char blended_stripes[EXA_MAXSIZE_ADMIND_PROP + 1];
};


/** Informations about a volume */
/* Warning keep aligned on 64 bits */
struct vrt_volume_info
{
    /** UUID of the group to wich this volume belongs */
    exa_uuid_t group_uuid;

    /** UUID of the volume */
    exa_uuid_t uuid;

    /** Volume name */
    char name [EXA_MAXSIZE_VOLUMENAME+1];

    /** Volume status */
    exa_volume_status_t status;

    /** Size in kb */
    uint64_t size;
};

/** Information about a real device */
struct vrt_realdev_info
{
    /** UUID of the group to wich this real device belongs */
    exa_uuid_t group_uuid;

    /** UUID of the real device */
    exa_uuid_t uuid;

    /** Physical size in KB */
    uint64_t size;

    /** Physical capacity used in KB */
    uint64_t capacity_used;

    /** Rdev status */
    exa_realdev_status_t status;

    uint32_t pad;

};

struct vrt_realdev_rebuild_info
{
    /** Size to rebuild */
    uint64_t size_to_rebuild;

    /** Size already rebuilt */
    uint64_t rebuilt_size;
};

/** Information about a real device */
struct vrt_realdev_reintegrate_info
{
    /** true if we need to run a reintegrate */
    bool reintegrate_needed;
};

#endif /* __VRT_COMMON_H__ */
