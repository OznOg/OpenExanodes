/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "monitoring/md/src/md_trap_sender.h"
#include "monitoring/md/src/md_srv_com.h"
#include "log/include/log.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>


typedef struct {
    md_msg_event_trap_t traps[MD_TRAP_SENDER_QUEUE_SIZE];
    int32_t front;
    int32_t count;
} md_trap_queue_t;


md_trap_queue_t trap_queue;
os_thread_mutex_t trap_queue_mutex;


md_trap_sender_error_code_t md_trap_sender_enqueue(const md_msg_event_trap_t *trap)
{
    os_thread_mutex_lock(&trap_queue_mutex);
    if (trap_queue.count == MD_TRAP_SENDER_QUEUE_SIZE)
    {
	os_thread_mutex_unlock(&trap_queue_mutex);
	return MD_TRAP_SENDER_OVERFLOW;
    }
    trap_queue.traps[(trap_queue.front + trap_queue.count++) %
		     MD_TRAP_SENDER_QUEUE_SIZE] = *trap;
    os_thread_mutex_unlock(&trap_queue_mutex);
    return MD_TRAP_SENDER_SUCCESS;
}

bool md_trap_sender_queue_empty(void)
{
    bool empty;
    os_thread_mutex_lock(&trap_queue_mutex);
    empty = trap_queue.count == 0;
    os_thread_mutex_unlock(&trap_queue_mutex);
    return empty;
}

static md_trap_sender_error_code_t md_trap_sender_dequeue(md_msg_event_trap_t** trap)
{
    os_thread_mutex_lock(&trap_queue_mutex);
    if (trap_queue.count == 0)
    {
	os_thread_mutex_unlock(&trap_queue_mutex);
	return MD_TRAP_SENDER_UNDERFLOW;
    }
    *trap = &trap_queue.traps[trap_queue.front];
    --trap_queue.count;
    trap_queue.front = (trap_queue.front + 1) % MD_TRAP_SENDER_QUEUE_SIZE;
    os_thread_mutex_unlock(&trap_queue_mutex);

    return MD_TRAP_SENDER_SUCCESS;
}

void md_trap_sender_loop(void)
{
    md_com_msg_t *tx_msg;
    md_msg_event_trap_t* trap;
    md_trap_sender_error_code_t ret;

    os_millisleep(100);

    if (md_trap_sender_queue_empty())
	return;

    while (!md_trap_sender_queue_empty())
    {
	ret = md_trap_sender_dequeue(&trap);
	assert(ret == MD_TRAP_SENDER_SUCCESS);

	/* if agentx is dead, do not send anything, just dequeue (forget past events) */
	if (md_srv_com_is_agentx_alive() == false)
	    return;

	/* md_msg_agent_trap_t is an alias on md_msg_event_trap_t ;
	   forwards it directly */
	tx_msg = md_com_msg_alloc_tx(MD_MSG_AGENT_TRAP,
				     (const char*) trap,
				     sizeof(md_msg_agent_trap_t));

	/* next send can block until trap is reported to be successfully sent.
	 * meanwhile, the queue might be filled; then enqueueing will report
	 * errors.
	 */
	md_srv_com_send_msg(tx_msg);

	md_com_msg_free_message(tx_msg);
    }
}

void md_trap_sender_static_init(void)
{
    /* FIXME memset is not a correcti init... */
    memset(&trap_queue, 0, sizeof(trap_queue));
    os_thread_mutex_init(&trap_queue_mutex);
}

void md_trap_sender_static_clean(void)
{
    /* kill buffer */
    memset(&trap_queue, 0xEE, sizeof(trap_queue));
    os_thread_mutex_destroy(&trap_queue_mutex);
}

void md_trap_sender_thread(void *pstop)
{
    bool *stop = (bool *)pstop;
    while (!*stop)
    {
	md_trap_sender_loop();
    }
}
