/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/include/daemon_api_client.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "vrt/virtualiseur/include/vrt_msg.h"


static int
vrt_client_generic_group_ev (ExamsgHandle h, const exa_uuid_t * group_uuid, int event)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int retval;

    exalog_debug("sending group event message to vrt");

    memset(&req.d.vrt_group_event, 0, sizeof(req.d.vrt_group_event));


    req.type  = VRTRECV_GROUP_EVENT;
    req.d.vrt_group_event.event = event;
    uuid_copy(&req.d.vrt_group_event.group_uuid, group_uuid);

    retval = admwrk_daemon_query (h, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				  &req, sizeof(req),
				  &reply,  sizeof(reply));
    if (retval < 0)
    {
	if (retval == -ADMIND_ERR_NODE_DOWN)
	    exalog_debug("Interrupted when trying to send group event message");
	else
	    exalog_error("Couldn't send group event message (%d)", retval);
	return retval;
    }

    return reply.retval;
}

int
vrt_client_nodes_status (ExamsgHandle h, exa_nodeset_t *nodes_up)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int retval;

    memset(&req.d.vrt_set_nodes_status, 0, sizeof(req.d.vrt_set_nodes_status));

    req.type = VRTRECV_NODE_SET_UPNODES;
    exa_nodeset_copy(&req.d.vrt_set_nodes_status.nodes_up, nodes_up);

    retval = admwrk_daemon_query_nointr(h, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
					&req, sizeof(req),
					&reply,  sizeof(reply));
    if (retval != 0)
    {
	exalog_error("Couldn't send device message (%d)", retval);
	return retval;

    }

    return reply.retval;
}

static int
vrt_client_generic_device_ev (ExamsgHandle h, const exa_uuid_t * group_uuid,
			      const exa_uuid_t *disk_uuid, int event)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int retval;

    memset (& req.d.vrt_device_event, 0, sizeof(req.d.vrt_device_event));

    exalog_debug("sending %d message to vrt", event);

    req.type = VRTRECV_DEVICE_EVENT;
    req.d.vrt_device_event.event    = event;
    uuid_copy (&req.d.vrt_device_event.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_device_event.rdev_uuid, disk_uuid);

    retval = admwrk_daemon_query (h, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				  &req, sizeof(req),
				  &reply, sizeof(reply));
    if (retval != 0)
    {
	if (retval == -ADMIND_ERR_NODE_DOWN)
	    exalog_debug("Interrupted when trying to send group event message");
	else
	    exalog_error("Couldn't send device message (%d)", retval);
	return retval;

    }

    return reply.retval;
}

int vrt_client_device_up (ExamsgHandle h, const exa_uuid_t * group_uuid,
			  const exa_uuid_t *rdev_uuid)
{
    return vrt_client_generic_device_ev (h, group_uuid, rdev_uuid,
					 VRT_DEVICE_UP);
}

int vrt_client_device_down (ExamsgHandle h, const exa_uuid_t * group_uuid,
			    const exa_uuid_t *rdev_uuid)
{
    return vrt_client_generic_device_ev (h, group_uuid, rdev_uuid,
					 VRT_DEVICE_DOWN);
}

int vrt_client_device_reintegrate(ExamsgHandle h, const exa_uuid_t * group_uuid,
				  const exa_uuid_t *rdev_uuid)
{
    return vrt_client_generic_device_ev(h, group_uuid, rdev_uuid,
                                        VRT_DEVICE_REINTEGRATE);
}

int vrt_client_device_post_reintegrate(ExamsgHandle h, const exa_uuid_t * group_uuid,
				       const exa_uuid_t *rdev_uuid)
{
    return vrt_client_generic_device_ev(h, group_uuid, rdev_uuid,
                                        VRT_DEVICE_POST_REINTEGRATE);
}

int vrt_client_device_replace(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                              const exa_uuid_t *vrt_uuid,
                              const exa_uuid_t *rdev_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    uuid_copy(&req.d.vrt_device_replace.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_device_replace.vrt_uuid, vrt_uuid);
    uuid_copy(&req.d.vrt_device_replace.rdev_uuid, rdev_uuid);

    req.type = VRTRECV_DEVICE_REPLACE;

    ret = admwrk_daemon_query(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
                              &req, sizeof(req), &reply, sizeof(reply));
    if (ret != 0)
    {
        exalog_debug("admwrk_daemon_query failed with %d", ret);
        return ret;
    }

    return reply.retval;
}

int vrt_client_group_suspend (ExamsgHandle h, const exa_uuid_t * group_uuid)
{
    return vrt_client_generic_group_ev (h, group_uuid, VRT_GROUP_SUSPEND);
}

int vrt_client_group_resume (ExamsgHandle h, const exa_uuid_t * group_uuid)
{
    return vrt_client_generic_group_ev (h, group_uuid, VRT_GROUP_RESUME);
}

int vrt_client_group_suspend_metadata_and_rebuild(ExamsgHandle h,
                                                  const exa_uuid_t *group_uuid)
{
    return vrt_client_generic_group_ev(h, group_uuid,
            VRT_GROUP_SUSPEND_METADATA_AND_REBUILD);
}

int vrt_client_group_resume_metadata_and_rebuild(ExamsgHandle h,
                                                 const exa_uuid_t *group_uuid)
{
    return vrt_client_generic_group_ev(h, group_uuid,
            VRT_GROUP_RESUME_METADATA_AND_REBUILD);
}

int vrt_client_group_compute_status (ExamsgHandle h, const exa_uuid_t * group_uuid)
{
    return vrt_client_generic_group_ev (h, group_uuid, VRT_GROUP_COMPUTESTATUS);
}

int vrt_client_group_wait_initialized_requests (ExamsgHandle h, const exa_uuid_t * group_uuid)
{
    return vrt_client_generic_group_ev (h, group_uuid, VRT_GROUP_WAIT_INITIALIZED_REQUESTS);
}

int vrt_client_get_volume_status (ExamsgHandle mh, const exa_uuid_t *group_uuid,
				  const exa_uuid_t *volume_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    uuid_copy(&req.d.vrt_get_volume_status.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_get_volume_status.volume_uuid, volume_uuid);

    req.type = VRTRECV_GET_VOLUME_STATUS;

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));

    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    /* FIXME: the volume status should not be carried by the same member as the
     * error code */
    return reply.retval;
}


int vrt_client_group_resync(ExamsgHandle h, const exa_uuid_t *group_uuid,
                            const exa_nodeset_t *nodes)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int err;

    uuid_copy(&req.d.vrt_group_resync.group_uuid, group_uuid);
    exa_nodeset_copy(&req.d.vrt_group_resync.nodes, nodes);

    req.type = VRTRECV_GROUP_RESYNC;

    err = admwrk_daemon_query_nointr(h, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
                                     &req, sizeof(req), &reply, sizeof(reply));

    return err ? err : reply.retval;
}


int vrt_client_group_post_resync (ExamsgHandle h, const exa_uuid_t * group_uuid)
{
    return vrt_client_generic_group_ev (h, group_uuid, VRT_GROUP_POSTRESYNC);
}


int vrt_client_group_start(ExamsgHandle mh, const exa_uuid_t *group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_START;

    uuid_copy(&req.d.vrt_group_start.group_uuid, group_uuid);
    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));

    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


int vrt_client_group_create(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                            uint32_t slot_width, uint32_t chunk_size, uint32_t su_size,
                            uint32_t dirty_zone_size, uint32_t blended_stripes,
                            uint32_t nb_spare, char *error_msg)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_CREATE;

    uuid_copy(&req.d.vrt_group_create.group_uuid, group_uuid);
    req.d.vrt_group_create.slot_width      = slot_width;
    req.d.vrt_group_create.chunk_size      = chunk_size;
    req.d.vrt_group_create.su_size         = su_size;
    req.d.vrt_group_create.dirty_zone_size = dirty_zone_size;
    req.d.vrt_group_create.blended_stripes = blended_stripes;
    req.d.vrt_group_create.nb_spare        = nb_spare;

    ret = admwrk_daemon_query(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
                              &req, sizeof(req),
                              &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    strlcpy(error_msg, reply.group_create.error_msg, EXA_MAXSIZE_LINE + 1);

    return reply.retval;
}

int vrt_client_group_insert_rdev(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                                 exa_nodeid_t node_id, spof_id_t spof_id,
                                 exa_uuid_t *uuid, exa_uuid_t *nbd_uuid,
                                 int local, uint64_t old_sb_version,
                                 uint64_t new_sb_version)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_INSERT_RDEV;

    uuid_copy(&req.d.vrt_group_insert_rdev.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_group_insert_rdev.uuid,       uuid);
    uuid_copy(&req.d.vrt_group_insert_rdev.nbd_uuid,   nbd_uuid);

    req.d.vrt_group_insert_rdev.old_sb_version = old_sb_version;
    req.d.vrt_group_insert_rdev.new_sb_version = new_sb_version;
    req.d.vrt_group_insert_rdev.node_id        = node_id;
    req.d.vrt_group_insert_rdev.spof_id        = spof_id;
    req.d.vrt_group_insert_rdev.local          = local;

    ret = admwrk_daemon_query(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			      &req, sizeof(req),
			      &reply, sizeof(reply));

    return ret == 0 ? reply.retval : ret;
}


int vrt_client_group_begin (ExamsgHandle mh, const char * group_name,
			    const exa_uuid_t * group_uuid, const char *layout_name,
                            uint64_t sb_version)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_BEGIN;

    strlcpy(req.d.vrt_group_begin.group_name, group_name,
	    sizeof(req.d.vrt_group_begin.group_name));
    strlcpy(req.d.vrt_group_begin.layout, layout_name,
	    sizeof(req.d.vrt_group_begin.layout));
    uuid_copy(&req.d.vrt_group_begin.group_uuid, group_uuid);
    req.d.vrt_group_begin.sb_version = sb_version;

    ret = admwrk_daemon_query_nointr (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				      &req, sizeof(req),
				      &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}

/*
 * FIXME: the node_id is used for SPOF here, it should made more clear
 */

int vrt_client_group_add_rdev (ExamsgHandle mh,
			       const exa_uuid_t *group_uuid,
			       exa_nodeid_t node_id, spof_id_t spof_id,
                               exa_uuid_t *uuid, exa_uuid_t *nbd_uuid,
			       int local, bool up)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_ADD_RDEV;

    uuid_copy(&req.d.vrt_group_add_rdev.group_uuid, group_uuid);
    req.d.vrt_group_add_rdev.node_id = node_id;
    req.d.vrt_group_add_rdev.spof_id = spof_id;
    uuid_copy(&req.d.vrt_group_add_rdev.uuid, uuid);
    uuid_copy(&req.d.vrt_group_add_rdev.nbd_uuid, nbd_uuid);
    req.d.vrt_group_add_rdev.local        = local;
    req.d.vrt_group_add_rdev.up           = up;

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


/**
 * Stop a group of the given name and with the given uuid.
 *
 * @param[in] name Name of the group to stop
 *
 * @param[in] group_uuid uuid of the group to stop
 *
 * @return         Exanodes error code
 */
int
vrt_client_group_stop(ExamsgHandle mh, const exa_uuid_t *group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_STOP;
    uuid_copy(&req.d.vrt_group_stop.group_uuid, group_uuid);

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


/**
 * Tests whether a group is stoppable.
 *
 * @param[in] group_uuid uuid of the group to stop
 *
 * @return EXA_SUCCESS on success, a positive error code on failure
 */
int
vrt_client_group_stoppable(ExamsgHandle mh,  const exa_uuid_t * group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_STOPPABLE;
    uuid_copy(&req.d.vrt_group_stoppable.group_uuid, group_uuid);

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


/**
 * Tests whether a group will go offline if we stop the specified
 * nodes
 *
 * @param[in] group_uuid   UUID of the group to stop
 *
 * @param[in] stop_nodes   The node that will stop
 *
 * @return EXA_SUCCESS on success, a positive error code on failure
 */
int
vrt_client_group_going_offline(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                               const exa_nodeset_t *stop_nodes)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_GOING_OFFLINE;
    uuid_copy(&req.d.vrt_group_going_offline.group_uuid, group_uuid);
    exa_nodeset_copy(&req.d.vrt_group_going_offline.stop_nodes, stop_nodes);

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


int vrt_client_group_sync_sb(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                             uint64_t old_sb_version, uint64_t new_sb_version)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_SYNC_SB;
    uuid_copy(&req.d.vrt_group_sync_sb.group_uuid, group_uuid);
    req.d.vrt_group_sync_sb.old_sb_version = old_sb_version;
    req.d.vrt_group_sync_sb.new_sb_version = new_sb_version;

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


/**
 * Freeze the group
 *
 * @param[in] group_uuid      UUID of the group to freeze
 *
 * @return EXA_SUCCESS on success, a positive error code on failure
 */
int
vrt_client_group_freeze(ExamsgHandle mh, const exa_uuid_t *group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_FREEZE;

    uuid_copy(&req.d.vrt_group_freeze.group_uuid, group_uuid);

    ret = admwrk_daemon_query(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			      &req, sizeof(req),
			      &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


/**
 * Unfreeze the group
 *
 * @param[in] group_uuid        UUID of the group to unfreeze
 *
 * @return EXA_SUCCESS on success, a positive error code on failure
 */
int
vrt_client_group_unfreeze(ExamsgHandle mh, const exa_uuid_t *group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_GROUP_UNFREEZE;

    uuid_copy(&req.d.vrt_group_unfreeze.group_uuid, group_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


int
vrt_client_volume_create(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                         const char *volume_name, const exa_uuid_t *volume_uuid,
                         uint64_t size)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_VOLUME_CREATE;

    uuid_copy(&req.d.vrt_volume_create.group_uuid, group_uuid);
    strlcpy(req.d.vrt_volume_create.volume_name, volume_name,
	    sizeof(req.d.vrt_volume_create.volume_name));
    uuid_copy(&req.d.vrt_volume_create.volume_uuid, volume_uuid);
    req.d.vrt_volume_create.volume_size = size;

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}

int
vrt_client_volume_start (ExamsgHandle mh, const exa_uuid_t *group_uuid,
			 const exa_uuid_t *volume_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_VOLUME_START;

    uuid_copy(&req.d.vrt_volume_start.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_volume_start.volume_uuid, volume_uuid);

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}

int
vrt_client_volume_stop (ExamsgHandle mh, const exa_uuid_t *group_uuid,
			const exa_uuid_t *volume_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_VOLUME_STOP;

    uuid_copy(&req.d.vrt_volume_stop.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_volume_stop.volume_uuid, volume_uuid);

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


int
vrt_client_volume_delete (ExamsgHandle mh, const exa_uuid_t *group_uuid,
			  const exa_uuid_t *volume_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_VOLUME_DELETE;

    uuid_copy(&req.d.vrt_volume_delete.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_volume_delete.volume_uuid, volume_uuid);

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


int
vrt_client_volume_resize (ExamsgHandle mh, const exa_uuid_t *group_uuid,
			  const exa_uuid_t *volume_uuid, uint64_t size)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_VOLUME_RESIZE;

    uuid_copy(&req.d.vrt_volume_resize.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_volume_resize.volume_uuid, volume_uuid);
    req.d.vrt_volume_resize.volume_size = size;

    ret = admwrk_daemon_query (mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
			       &req, sizeof(req),
			       &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query failed with %d", ret);
	return ret;
    }

    return reply.retval;
}


int
vrt_client_group_info (ExamsgHandle mh, const exa_uuid_t *group_uuid,
		       struct vrt_group_info *group_info)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req, 0, sizeof(req));

    req.type = VRTRECV_ASK_INFO;
    req.d.vrt_ask_info.type = GROUP_INFO;
    uuid_copy(&req.d.vrt_ask_info.group_uuid, group_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply,  sizeof(reply));

    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query_nointr failed with %d", ret);
	return ret;
    }

    *group_info = reply.group_info;

    return reply.retval;
}


int
vrt_client_volume_info (ExamsgHandle mh, const exa_uuid_t *group_uuid,
			const exa_uuid_t *volume_uuid,
			struct vrt_volume_info *volume_info)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req, 0, sizeof(req));

    req.type = VRTRECV_ASK_INFO;
    req.d.vrt_ask_info.type = VOLUME_INFO;
    uuid_copy(&req.d.vrt_ask_info.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_ask_info.volume_uuid, volume_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));

    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query_nointr failed with %d", ret);
	return ret;
    }

    if (reply.retval != EXA_SUCCESS)
        return reply.retval;

    *volume_info = reply.volume_info;

    if (uuid_is_zero(&volume_info->group_uuid) ||
	uuid_is_zero(&volume_info->uuid))
	return -ENOENT;

    return EXA_SUCCESS;
}


int
vrt_client_rdev_info (ExamsgHandle mh, const exa_uuid_t *group_uuid,
		      const exa_uuid_t *rdev_uuid,
		      struct vrt_realdev_info *realdev_info)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req, 0, sizeof(req));

    req.type = VRTRECV_ASK_INFO;
    req.d.vrt_ask_info.type = RDEV_INFO;
    uuid_copy(&req.d.vrt_ask_info.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_ask_info.disk_uuid, rdev_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));

    if (ret != 0)
    {
	exalog_debug("admwrk_daemon_query_nointr failed with %d", ret);
	return ret;
    }

    if (reply.retval != EXA_SUCCESS)
        return reply.retval;

    *realdev_info = reply.rdev_info;

    if (uuid_is_zero(&realdev_info->group_uuid) ||
	uuid_is_zero(&realdev_info->uuid))
	return -ENOENT;

    return EXA_SUCCESS;
}


int vrt_client_rdev_rebuild_info(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                                 const exa_uuid_t *rdev_uuid,
                                 struct vrt_realdev_rebuild_info *rebuild_info)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req, 0, sizeof(req));

    req.type = VRTRECV_ASK_INFO;
    req.d.vrt_ask_info.type = RDEV_REBUILD_INFO;
    uuid_copy(&req.d.vrt_ask_info.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_ask_info.disk_uuid, rdev_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));

    if (ret != 0)
	return ret;

    *rebuild_info = reply.rdev_rebuild_info;

    return reply.retval;
}


int
vrt_client_rdev_reintegrate_info (ExamsgHandle mh, const exa_uuid_t *group_uuid,
                                  const exa_uuid_t  *rdev_uuid,
                                  struct vrt_realdev_reintegrate_info *realdev_info)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req, 0, sizeof(req));

    req.type = VRTRECV_ASK_INFO;
    req.d.vrt_ask_info.type = RDEV_REINTEGRATE_INFO;
    uuid_copy(&req.d.vrt_ask_info.group_uuid, group_uuid);
    uuid_copy(&req.d.vrt_ask_info.disk_uuid, rdev_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));

    *realdev_info = reply.rdev_reintegrate_info;

    return ret == 0 ? reply.retval : ret;
}


int vrt_client_group_reset(ExamsgHandle mh, const exa_uuid_t *group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req.d.vrt_group_reset, 0, sizeof(req.d.vrt_group_reset));
    req.type = VRTRECV_GROUP_RESET;
    uuid_copy(&req.d.vrt_group_reset.group_uuid, group_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
                                     &req, sizeof(req),
                                     &reply, sizeof(reply));
    if (ret != 0)
    {
        exalog_debug("admwrk_daemon_query_nointr failed with %d", ret);
        return ret;
    }

    return reply.retval;
}


int vrt_client_group_check(ExamsgHandle mh, const exa_uuid_t *group_uuid)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    memset(&req.d.vrt_group_check, 0, sizeof(req.d.vrt_group_check));
    req.type = VRTRECV_GROUP_CHECK;
    uuid_copy(&req.d.vrt_group_check.group_uuid, group_uuid);

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
                                     &req, sizeof(req),
                                     &reply, sizeof(reply));
    if (ret != 0)
    {
        exalog_debug("admwrk_daemon_query_nointr failed with %d", ret);
        return ret;
    }

    return reply.retval;
}


int vrt_client_stat_get(ExamsgHandle mh, struct vrt_stats_request *stats_request,
                        struct vrt_stats_reply *stats)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_STATS;
    req.d.vrt_stats_request = *stats_request;

    ret =  admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				      &req, sizeof(req),
				      &reply, sizeof(reply));

    *stats = reply.stats;

    if (ret != 0)
	return ret;

    return reply.retval;
}


int vrt_client_pending_group_cleanup(ExamsgHandle mh)
{
    vrt_cmd_t req;
    vrt_reply_t reply;
    int ret;

    req.type = VRTRECV_PENDING_GROUP_CLEANUP;

    ret = admwrk_daemon_query_nointr(mh, EXAMSG_VRT_ID, EXAMSG_DAEMON_RQST,
				     &req, sizeof(req),
				     &reply, sizeof(reply));
    if (ret != 0)
    {
	exalog_error("admwrk_daemon_query_nointr failed with %d", ret);
	return ret;
    }

    return reply.retval;
}

