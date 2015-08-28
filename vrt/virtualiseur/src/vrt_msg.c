/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/**@file vrt_msg.c
 *
 * @brief This file contains the virtualizer messaging subsystem. It
 * receives examsg messages, perform the required operation and
 * eventually replies to the messages.
 */

#include <string.h>

#include "examsg/include/examsg.h"
#include "common/include/exa_error.h"
#include "common/include/threadonize.h"


#include "vrt/virtualiseur/include/vrt_cmd.h"
#include "vrt/virtualiseur/include/vrt_cmd_threads.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_msg.h"

#include "admind/include/evmgr_pub_events.h"
#include "log/include/log.h"
#include "common/include/daemon_request_queue.h"
#include "common/include/daemon_api_server.h"

#include "nbd/service/include/nbd_msg.h"

#include "vrt/virtualiseur/src/vrt_module.h"

#include "os/include/os_error.h"
#include "os/include/os_thread.h"

/** The Examsg handle used to send (un)locking requests to the local
    NBD server and receive their answers */
static ExamsgHandle vrt_msg_locking_handle;

/** The vrt_msg_locking_handle must be protected against concurrent
    accesses, because multiple rebuilding threads can be running on
    this node */
static os_thread_mutex_t vrt_msg_locking_handle_lock;

/** The Examsg handle used to receive commands from admind, and send
    their answers ; this handle is also used to send traps to monitoring daemon */
ExamsgHandle vrt_msg_handle;

/** Multiplexer thread */
static struct {
    os_thread_t id;
    bool must_die;
} multiplexer_thread;

/**
 * Multiplexer thread for all virtualizer messages. As we don't want to block
 * this thread (which would delay delivery of other messages), we just wakeup
 * the virtualizer thread, which will later process the pending message.
 *
 * @param[in] data     The messaging handle
 */
static void vrt_msg_multiplexer_thread_loop(void *data)
{
    ExamsgHandle mh = (ExamsgHandle) data;

    exalog_as(EXAMSG_VRT_ID);

    while (1)
    {
	struct daemon_request_queue *queue = NULL;
	int retval;
	struct timeval tv;
	Examsg msg;
	ExamsgMID from;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	retval = examsgWaitInterruptible(mh, &tv);
	if (retval != 0 && multiplexer_thread.must_die)
	    break;

	if (retval != 0)
	    continue;

	retval = examsgRecv(vrt_msg_handle, &from, &msg, sizeof(msg));

	if (retval == 0)
	    continue;

	if (retval < 0)
	{
	    exalog_error("Error during message reception, skipping (%d)", retval);
	    continue;
	}

	queue = vrt_cmd_thread_queue_get(from.id);
	EXA_ASSERT(queue != NULL);

	switch (msg.any.type)
	{
	case EXAMSG_DAEMON_INTERRUPT:
	    daemon_request_queue_add_interrupt(queue, mh, from.id);
	    break;

	case EXAMSG_DAEMON_RQST:
	    {
		const vrt_cmd_t *recv = (const vrt_cmd_t *)msg.payload;
                vrt_reply_t reply;

		if (recv->type == VRTRECV_GROUP_UNFREEZE)
		{
		    /* Handle group unfreeze directly in the multiplexer thread because
		     * it is used for rollbacking a group freeze during commands. It is
		     * OK to execute it here since it cannot block.
		     * */
		    reply.retval = vrt_cmd_group_unfreeze(&recv->d.vrt_group_unfreeze);

		    EXA_ASSERT(admwrk_daemon_reply(mh, from.id,
                                                   &reply, sizeof(reply)) == 0);
		    break;
		}
		else if (recv->type == VRTRECV_GROUP_EVENT
			 && recv->d.vrt_group_event.event == VRT_GROUP_SUSPEND)
		{
		    /* Handle group_suspend directly in the multiplexer thread since we
		       have to accept it even if a recovery command is in
		       progress. It is OK to execute it here since it cannot
		       block. */
		    struct vrt_group *group;
		    const exa_uuid_t *uuid = &recv->d.vrt_group_event.group_uuid;

		    group = vrt_get_group_from_uuid(uuid);
		    if (!group)
		    {
			exalog_debug("group " UUID_FMT " not found", UUID_VAL(uuid));
			reply.retval = -VRT_ERR_UNKNOWN_GROUP_UUID;
		    }
		    else
		    {
			reply.retval = vrt_group_suspend(group);
			vrt_group_unref(group);
		    }

		    EXA_ASSERT(admwrk_daemon_reply(mh, from.id,
                                                   &reply, sizeof(reply)) == 0);

		    continue;
		}
		else
		    daemon_request_queue_add_request(queue,
			    recv, sizeof(*recv), from.id);
	    }
	    break;

	default:
	    EXA_ASSERT_VERBOSE(FALSE, "Message type %d not handled", msg.any.type);
	}
    }
}


int vrt_msg_reintegrate_device(void)
{
    instance_event_msg_t msg;
    int r;

    msg.any.type = EXAMSG_EVMGR_INST_EVENT;
    msg.event.id = EXAMSG_VRT_ID;
    msg.event.state = INSTANCE_CHECK_UP;
    msg.event.node_id = vrt_node_get_local_id();

    r = examsgSend(vrt_msg_handle, EXAMSG_ADMIND_EVMGR_ID, EXAMSG_ALLHOSTS,
		   &msg, sizeof(msg));
    if (r != sizeof(msg))
	return r;

    return EXA_SUCCESS;
}


/**
 * Ask the local NBD to lock or unlock writes on part of a given
 * device.
 *
 * @param[in] nbd uuid  UUID of the real device in the NBD
 *
 * @param[in] start Start of the area (in sectors)
 *
 * @param[in] size  Size of the area (in sectors)
 *
 * @param[in] lock  Boolean that tells whether it is a lock request
 *                  (TRUE) or a unlock request (FALSE)
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
int
vrt_msg_nbd_lock (exa_uuid_t *nbd_uuid, uint64_t start, uint64_t size, int lock)
{
    ExamsgNbdLock msg;
    int ret, error;

    memset (& msg, 0, sizeof(msg));

    msg.any.type = EXAMSG_NBD_LOCK;
    uuid_copy(&msg.disk_uuid, nbd_uuid);
    msg.locked_zone_start = start;
    msg.locked_zone_size  = size;
    msg.lock              = lock;

    os_thread_mutex_lock(&vrt_msg_locking_handle_lock);

    ret = examsgSendWithAck (vrt_msg_locking_handle, EXAMSG_NBD_LOCKING_ID,
			     EXAMSG_LOCALHOST, (Examsg*) & msg, sizeof (msg),
			     &error);

    os_thread_mutex_unlock(&vrt_msg_locking_handle_lock);

    if (ret != sizeof (msg))
	return ret;

    return error;
}


/**
 * Initialize the virtualizer messaging subsystem
 *
 * @return 0 on success, -1 on error
 */
int
vrt_msg_subsystem_init (void)
{
    int ret;

    os_thread_mutex_init(&vrt_msg_locking_handle_lock);

    vrt_msg_locking_handle = examsgInit (EXAMSG_VRT_LOCKING_ID);
    if (vrt_msg_locking_handle == NULL)
    {
	exalog_error("Error while examsgInit()");
	ret = -ENOMEM;
	goto error_init_lock;
    }

    ret = examsgAddMbox(vrt_msg_locking_handle,
	                examsgOwner(vrt_msg_locking_handle),
			1, EXAMSG_MSG_MAX);
    if (ret != 0)
    {
	exalog_error("Error while creating lock mailbox");
	goto error_create_lock;
    }

    vrt_msg_handle = examsgInit (EXAMSG_VRT_ID);
    if (vrt_msg_handle == NULL)
    {
	exalog_error("Error while examsgInit()");
	ret = -ENOMEM;
	goto error_init;
    }

    ret = examsgAddMbox(vrt_msg_handle, examsgOwner(vrt_msg_handle),
	                3, EXAMSG_MSG_MAX);
    if (ret != 0)
    {
	exalog_error("Error while creating mailbox");
	goto error_create;
    }

    multiplexer_thread.must_die = false;
    if (!exathread_create_named(&multiplexer_thread.id,
                                VRT_THREAD_STACK_SIZE,
                                vrt_msg_multiplexer_thread_loop,
                                vrt_msg_handle, "vrt_msg"))
    {
	exalog_error("Error while creating thread");
        ret = -EXA_ERR_DEFAULT;
	goto error_thread;
    }

    return EXA_SUCCESS;

error_thread:
    examsgDelMbox(vrt_msg_handle, EXAMSG_VRT_ID);
error_create:
    examsgExit(vrt_msg_handle);
error_init:
    examsgDelMbox(vrt_msg_locking_handle, EXAMSG_VRT_LOCKING_ID);
error_create_lock:
    examsgExit(vrt_msg_locking_handle);
error_init_lock:
    return ret;
}


/**
 * Cleanup the virtualizer messaging subsystem
 */
void vrt_msg_subsystem_cleanup(void)
{
    multiplexer_thread.must_die = true;
    os_thread_join(multiplexer_thread.id);
    multiplexer_thread.must_die = false;

    examsgDelMbox(vrt_msg_handle, EXAMSG_VRT_ID);
    examsgExit(vrt_msg_handle);

    examsgDelMbox(vrt_msg_locking_handle, EXAMSG_VRT_LOCKING_ID);
    examsgExit(vrt_msg_locking_handle);
}
