/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_cmd.h"
#include "vrt/virtualiseur/include/vrt_info.h"
#include "vrt/virtualiseur/include/vrt_stats.h"


#include "examsg/include/examsg.h"

#include "common/include/exa_error.h"
#include "common/include/threadonize.h"
#include "common/include/daemon_request_queue.h"

#include "os/include/os_mem.h"

#define VRT_CMD_THREAD_NAME_MAXLEN 15

struct vrt_cmd_thread
{
    os_thread_t tid;
    struct daemon_request_queue *queue;
    bool ask_terminate;
    ExamsgHandle mh;
    vrt_reply_t *reply;
};

static struct vrt_cmd_thread *cmd_thread;
static struct vrt_cmd_thread *info_thread;
static struct vrt_cmd_thread *recover_thread;

extern ExamsgHandle vrt_msg_handle;

static void vrt_cmd_thread_loop(void *data)
{
    struct vrt_cmd_thread *th = data;

    exalog_as(EXAMSG_VRT_ID);

    while (1)
    {
	vrt_cmd_t recv;
	ExamsgID from;
	int ret;

	/* A signal has interrupted us */
	if (daemon_request_queue_get_request(th->queue, &recv,
					     sizeof(recv), &from) != 0)
	{
	    /* Have we been ask to terminate ? */
	    if (th->ask_terminate)
		break;
	    else
		continue;
	}
	/* Dispatch the message to the correct subsystem */
	if (recv.type == VRTRECV_ASK_INFO)
	    vrt_info_handle_message(&recv.d.vrt_ask_info, th->reply);
	else if (recv.type == VRTRECV_STATS)
	    vrt_stats_handle_message(&recv.d.vrt_stats_request, th->reply);
	else
	    vrt_cmd_handle_message(&recv, th->reply);

	ret = daemon_request_queue_reply(vrt_msg_handle, from,
					 th->queue, th->reply, sizeof(*th->reply));
	if (ret != EXA_SUCCESS)
	    exalog_error("VRT reply failed, %d", ret);
    }
}

/**
 * Create a VRT thread.
 *
 * @param[in]   name        The thread's name
 * @param[in]   stack_size  The thread's stack size
 */
static struct vrt_cmd_thread *
vrt_cmd_thread_create(const char *name, size_t stack_size)
{
    struct vrt_cmd_thread *th;

    th = os_malloc(sizeof(struct vrt_cmd_thread));
    if (! th)
	return NULL;

    th->reply = os_malloc(sizeof(vrt_reply_t));
    if (! th->reply)
    {
	os_free(th);
	return NULL;
    }

    th->ask_terminate = false;
    th->queue = daemon_request_queue_new(name);
    if (! th->queue)
    {
	os_free(th->reply);
	os_free(th);
	return NULL;
    }

    if (!exathread_create_named(&th->tid, stack_size,
                                vrt_cmd_thread_loop, th, name))
    {
	daemon_request_queue_delete(th->queue);
	os_free(th->reply);
	os_free(th);
	return NULL;
    }

    return th;
}

static void vrt_cmd_thread_destroy(struct vrt_cmd_thread *th)
{
    th->ask_terminate = true;
    daemon_request_queue_break_get_request(th->queue);
    os_thread_join(th->tid);
    daemon_request_queue_delete(th->queue);
    os_free(th->reply);
    os_free(th);
}

struct daemon_request_queue *
vrt_cmd_thread_queue_get(ExamsgID id)
{
    switch(id)
    {
    case EXAMSG_ADMIND_INFO_LOCAL:
	return info_thread->queue;
    case EXAMSG_ADMIND_CMD_LOCAL:
	return cmd_thread->queue;
    case EXAMSG_ADMIND_RECOVERY_LOCAL:
	return recover_thread->queue;
    default:
	EXA_ASSERT_VERBOSE(FALSE, "Bad component %u\n", id);
    }

    return NULL;
}

int
vrt_cmd_threads_init(void)
{
    cmd_thread = vrt_cmd_thread_create("exa_vrt_cmd", VRT_THREAD_STACK_SIZE);

    if (! cmd_thread)
	return -ENOMEM;

    info_thread = vrt_cmd_thread_create("exa_vrt_info", VRT_THREAD_STACK_SIZE);
    if (! info_thread)
    {
	vrt_cmd_thread_destroy(cmd_thread);
	return -ENOMEM;
    }

    recover_thread = vrt_cmd_thread_create("exa_vrt_recover", VRT_THREAD_STACK_SIZE);
    if (! recover_thread)
    {
	vrt_cmd_thread_destroy(info_thread);
	vrt_cmd_thread_destroy(cmd_thread);
	return -ENOMEM;
    }

    return EXA_SUCCESS;
}

void
vrt_cmd_threads_cleanup(void)
{
    vrt_cmd_thread_destroy(recover_thread);
    vrt_cmd_thread_destroy(info_thread);
    vrt_cmd_thread_destroy(cmd_thread);
}
