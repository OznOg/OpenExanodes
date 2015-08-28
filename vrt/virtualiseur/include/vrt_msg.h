/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_MSG_H__
#define __VRT_MSG_H__

#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "common/include/exa_nodeset.h"
#include "vrt/common/include/spof.h"

#include "examsg/include/examsg.h"

#include "vrt/virtualiseur/include/vrt_volume_stats.h"
#include "vrt/virtualiseur/include/vrt_common.h"

/**
 * Message used by Admind to send request for information to the VRT
 */
struct VrtAskInfo {
    /** Type of information requested */
    enum { GROUP_INFO,
	   VOLUME_INFO,
	   RDEV_INFO,
	   RDEV_REBUILD_INFO,
	   RDEV_REINTEGRATE_INFO,
    } type;

    /** Group UUID (needed for GROUP_INFO, VOLUME_INFO, RDEV_INFO) */
    exa_uuid_t group_uuid;

    /** Volume UUID (needed for VOLUME_INFO) */
    exa_uuid_t volume_uuid;

    /** Disk UUID (needed for RDEV_INFO, RDEV_REBUILD_INFO, RDEV_REINTEGRATE_INFO) */
    exa_uuid_t disk_uuid;
};



/**
 * Message used by Admind to send group reset cmd to the virtualizer
 */
struct VrtGroupReset {
    exa_uuid_t group_uuid;
};



/**
 * Message used by Admind to send group check cmd to the virtualizer
 */
struct VrtGroupCheck {
    exa_uuid_t group_uuid;
};



/**
 * Message used by admind to set the status of a node to the virtualizer
 */
struct VrtSetNodesStatus {
    exa_nodeset_t nodes_up;
};

/**
 * Message used by admind to request a group action to the virtualizer
 */
struct VrtGroupEvent {
    /**
     * Type of the event.
     *
     * VRT_GROUP_SUSPEND is called to suspend all requests on the given
     * group.
     *
     * VRT_GROUP_COMPUTESTATUS is called to compute status after the VRT get the
     * new status of nodes and disks
     *
     * VRT_GROUP_RESUME is called to restart all requests from the user
     * previously suspended by a VRT_GROUP_SUSPEND event.
     *
     * VRT_GROUP_WAIT_INITIALIZED_REQUESTS is called to wait the completion of
     * requests issued before the suspend; they will be cancelled and
     * reinitialized
     *
     * VRT_GROUP_POSTRESYNC is called after the resync.
     */
    enum { VRT_GROUP_SUSPEND,
	   VRT_GROUP_RESUME,
	   VRT_GROUP_SUSPEND_METADATA_AND_REBUILD,
	   VRT_GROUP_RESUME_METADATA_AND_REBUILD,
	   VRT_GROUP_COMPUTESTATUS,
	   VRT_GROUP_WAIT_INITIALIZED_REQUESTS,
	   VRT_GROUP_POSTRESYNC
        } event;

    int pad;

    /** UUID of the group on which the event applies */
    exa_uuid_t group_uuid;
};


/**
 * Message used by admind to request a real device action to the virtualizer
 */
struct VrtDeviceEvent {
    /**
     * Type of the event
     *
     * VRT_DEVICE_DOWN is called to indicate that a device is not usable
     * anymore.
     *
     * VRT_DEVICE_UP is called to indicate that a device is usable
     * again, but not synchronized.
     *
     * VRT_DEVICE_REINTEGRATE is called at the end of the rebuilding to
     * ask the virtualizer to use this device again.
     *
     * VRT_DEVICE_POST_REINTEGRATE is called at the end of the rebuilding,
     * after the reintegrate and the sync of the superblocks to ask the
     * virtualizer to run another rebuilding.
     */
    enum
    {
	VRT_DEVICE_DOWN,
	VRT_DEVICE_UP,
	VRT_DEVICE_REINTEGRATE,
	VRT_DEVICE_POST_REINTEGRATE,
    } event;

    int pad;

    exa_uuid_t group_uuid;

    exa_uuid_t rdev_uuid;
};

struct VrtGroupBegin {
    char       group_name [EXA_MAXSIZE_GROUPNAME + 1];
    exa_uuid_t group_uuid;
    char       layout [EXA_MAXSIZE_LAYOUTNAME + 1];
    uint64_t   sb_version;
};

struct VrtGroupAddRdev {
    exa_uuid_t   group_uuid;
    exa_nodeid_t node_id;
    spof_id_t spof_id;
    exa_uuid_t uuid;
    exa_uuid_t nbd_uuid;
    uint32_t local;
    uint32_t up;
};

struct VrtGroupAddVolume {
    exa_uuid_t group_uuid;
    exa_uuid_t uuid;
    char       name[EXA_MAXSIZE_VOLUMENAME + 1];
    uint64_t   size;            /**< size in KB */
};

struct VrtGroupCreate {
    exa_uuid_t group_uuid;
    uint32_t slot_width;        /**< number of chunks per slot */
    uint32_t chunk_size;        /**< size of a chunk in KB */
    uint32_t su_size;           /**< size in KB */
    uint32_t dirty_zone_size;   /**< size in KB */
    uint32_t blended_stripes;
    uint32_t nb_spare;
};

struct VrtGroupStoppable {
    exa_uuid_t group_uuid;
};

struct VrtGroupGoingOffline {
    exa_uuid_t group_uuid;
    exa_nodeset_t stop_nodes;
};

struct VrtGroupStop {
    exa_uuid_t group_uuid;
};

struct VrtGroupStart {
    exa_uuid_t group_uuid;
};

struct VrtGroupInsertRdev {
    exa_uuid_t   group_uuid;
    exa_nodeid_t node_id;
    spof_id_t    spof_id;
    exa_uuid_t   uuid;
    exa_uuid_t   nbd_uuid;
    uint32_t     local;
    uint64_t     old_sb_version;
    uint64_t     new_sb_version;
};

struct VrtGroupSyncSb {
    exa_uuid_t group_uuid;
    uint64_t old_sb_version;
    uint64_t new_sb_version;
};

struct VrtGroupFreeze {
    exa_uuid_t group_uuid;
};

struct VrtGroupUnfreeze {
    exa_uuid_t group_uuid;
};

struct VrtVolumeCreate {
    exa_uuid_t group_uuid;
    char       volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    exa_uuid_t volume_uuid;
    uint64_t   volume_size;         /**< size in KB */
};

struct VrtVolumeStart {
    exa_uuid_t group_uuid;
    exa_uuid_t volume_uuid;
};

struct VrtVolumeStop {
    exa_uuid_t group_uuid;
    exa_uuid_t volume_uuid;
};

struct VrtVolumeResize {
    exa_uuid_t group_uuid;
    exa_uuid_t volume_uuid;
    uint64_t volume_size;    /**< New size of the volume, in KB */
};

struct VrtVolumeDelete {
    exa_uuid_t group_uuid;
    exa_uuid_t volume_uuid;
};

struct VrtDeviceReplace {
    exa_uuid_t group_uuid;
    exa_uuid_t vrt_uuid;
    exa_uuid_t rdev_uuid;
};

struct VrtDeviceReset {
    exa_uuid_t group_uuid;
    exa_uuid_t vrt_uuid;
};

struct VrtGetVolumeStatus {
    exa_uuid_t group_uuid;
    exa_uuid_t volume_uuid;
};

struct VrtVolumeStatReset {
    uint32_t stattype;
    uint32_t pad;
};

struct vrt_stats_request
{
    exa_bool_t reset;
    exa_uuid_t group_uuid;
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
};


struct vrt_stats_reply
{
    uint64_t last_reset;
    uint64_t now;
    struct vrt_stats_begin begin;
    struct vrt_stats_done done;
};

struct vrt_group_resync_request
{
    exa_uuid_t group_uuid;
    exa_nodeset_t nodes;
};

/* FIXME: group_create is the only command to return a specific error message
 *        maybe we should use only the error code and just log the specific
 *        error message.
 */
struct vrt_group_create
{
    char error_msg[EXA_MAXSIZE_LINE + 1];
};

#define VRT_ENGINE_EXAMSG_MAX_SIZE 64
struct VrtRequest {
    char      contents[VRT_ENGINE_EXAMSG_MAX_SIZE];
    uint64_t  node_from;
    uint64_t  node_dest;
    ExamsgID  dest;
    int       belongs_to_even_tokens;
    uint64_t  vrt_msg_op;
};

/**
 * Union of all the messages the virtualizer can receive
 */
typedef struct  VrtRecv
{
    enum {
#define VRTRECV_TYPE_FIRST VRTRECV_GROUP_EVENT
	VRTRECV_GROUP_EVENT = 200, /* Make sure we can tell this enum from examsgtype */
	VRTRECV_DEVICE_EVENT,
	VRTRECV_NODE_SET_UPNODES,
	VRTRECV_ASK_INFO,
	VRTRECV_GROUP_RESET,
	VRTRECV_GROUP_CHECK,
	VRTRECV_GROUP_BEGIN,
	VRTRECV_GROUP_ADD_RDEV,
	VRTRECV_GROUP_CREATE,
	VRTRECV_GROUP_START,
	VRTRECV_GROUP_STOP,
	VRTRECV_GROUP_INSERT_RDEV,
	VRTRECV_GROUP_STOPPABLE,
	VRTRECV_GROUP_GOING_OFFLINE,
	VRTRECV_GROUP_SYNC_SB,
	VRTRECV_GROUP_FREEZE,
	VRTRECV_GROUP_UNFREEZE,
	VRTRECV_VOLUME_CREATE,
	VRTRECV_VOLUME_START,
	VRTRECV_VOLUME_STOP,
	VRTRECV_VOLUME_RESIZE,
	VRTRECV_VOLUME_DELETE,
        VRTRECV_DEVICE_REPLACE,
	VRTRECV_GET_VOLUME_STATUS,
	VRTRECV_STATS,
        VRTRECV_GROUP_RESYNC,
	VRTRECV_PENDING_GROUP_CLEANUP
#define VRTRECV_TYPE_LAST VRTRECV_PENDING_GROUP_CLEANUP
    } type;
#define VRTRECV_TYPE_IS_VALID(t) ((t) <= VRTRECV_TYPE_LAST && (t) >= VRTRECV_TYPE_FIRST)

    union {
	struct VrtAskInfo                 vrt_ask_info;
	struct VrtGroupReset              vrt_group_reset;
	struct VrtGroupCheck              vrt_group_check;
	struct VrtSetNodesStatus          vrt_set_nodes_status;
	struct VrtGroupEvent              vrt_group_event;
	struct VrtDeviceEvent             vrt_device_event;
	struct VrtGroupBegin              vrt_group_begin;
	struct VrtGroupAddRdev            vrt_group_add_rdev;
	struct VrtGroupAddVolume          vrt_group_add_volume;
	struct VrtGroupCreate             vrt_group_create;
	struct VrtGroupInsertRdev         vrt_group_insert_rdev;
	struct VrtGroupStoppable          vrt_group_stoppable;
	struct VrtGroupGoingOffline       vrt_group_going_offline;
	struct VrtGroupStop               vrt_group_stop;
	struct VrtGroupStart              vrt_group_start;
	struct VrtGroupSyncSb 		  vrt_group_sync_sb;
	struct VrtGroupFreeze             vrt_group_freeze;
	struct VrtGroupUnfreeze           vrt_group_unfreeze;
	struct VrtVolumeCreate            vrt_volume_create;
	struct VrtVolumeStart             vrt_volume_start;
	struct VrtVolumeStop              vrt_volume_stop;
	struct VrtVolumeResize            vrt_volume_resize;
	struct VrtVolumeDelete            vrt_volume_delete;
        struct VrtDeviceReplace           vrt_device_replace;
	struct VrtDeviceReset             vrt_device_reset;
	struct VrtGetVolumeStatus         vrt_get_volume_status;
	struct VrtVolumeStatReset         vrt_volume_stat_reset;
	struct vrt_stats_request          vrt_stats_request;
        struct vrt_group_resync_request   vrt_group_resync;
    } d;
} vrt_cmd_t;

/**
 * Union of all the messages the virtualizer can send
 */
typedef struct {
    int retval;
    union {
        struct vrt_group_info group_info;
        struct vrt_volume_info volume_info;
        struct vrt_realdev_info rdev_info;
        struct vrt_realdev_rebuild_info rdev_rebuild_info;
        struct vrt_realdev_reintegrate_info rdev_reintegrate_info;
        struct vrt_stats_reply stats;
        struct vrt_group_create group_create;
    };
} vrt_reply_t;


int  vrt_msg_subsystem_init(void);
void vrt_msg_subsystem_cleanup(void);
int  vrt_msg_nbd_lock(exa_uuid_t *nbd_uuid, uint64_t start, uint64_t end,
                      int lock);

#endif /* __VRT_MSG_H__ */
