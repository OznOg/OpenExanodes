/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md/src/md_srv_com.h"
#include "monitoring/common/include/md_constants.h"
#include "monitoring/common/include/md_types.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"
#include "log/include/log.h"

#include <assert.h>
#include <stdbool.h>

static bool stop = false;

static int server_connection_id = -1;
static int client_connection_id = -1;
static os_thread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;
/* FIXME this lock does not know what it locks. BTW what is supposed to be
 * locked is badly done as this lock seems to prevent check_connection
 * function to be reentrant, when it should just protect sockets_id access.
 * This is clearly a problem of ownership on this sockets, maybe each sender
 * could have its own socket, or maybe this thread should behave as a proxy
 * and be the only one that access the socket.
 * This is also linked to the bad locking in the send function which prevent
 * send to be reentrant as well...
 * Deep rework is needed all over here... */

static struct timespec alive_msg_time;
static os_thread_mutex_t alive_msg_time_mutex = PTHREAD_MUTEX_INITIALIZER;

static void reset_alive_msg_time()
{
    os_thread_mutex_lock(&alive_msg_time_mutex);
    os_get_monotonic_time(&alive_msg_time);
    os_thread_mutex_unlock(&alive_msg_time_mutex);
}

static void handle_req_msg(md_com_msg_t* msg)
{
    /* TODO */
}

static void handle_alive_msg(md_com_msg_t* msg)
{
    /* reset alive timer */
    reset_alive_msg_time();

    /* ack with exactly the same message */
    md_srv_com_send_msg(msg);
}

static void handle_unknown_msg(md_com_msg_t* msg)
{
    exalog_error("exa_md received unknown message.");
}

static void close_connection()
{
    os_thread_mutex_lock(&connection_mutex);
    md_com_close(client_connection_id);
    client_connection_id = -1;
    os_thread_mutex_unlock(&connection_mutex);
}

static void check_connection()
{
    int _connection_id;
    md_com_error_code_t ret;

    os_thread_mutex_lock(&connection_mutex);

    if (server_connection_id == -1)
    {
	os_thread_mutex_unlock(&connection_mutex);

	ret = md_com_listen(MD_COM_SOCKET_PATH, &_connection_id);
	if (ret != COM_SUCCESS)
	{
	    exalog_error("exa_md cannot listen for client connections.");
	    return;
	}

	os_thread_mutex_lock(&connection_mutex);

	if (server_connection_id != -1)
	{
	    /* There is a race, another thread may have reopened and set the
	     * value while we were reconnecting.... */
	    os_thread_mutex_unlock(&connection_mutex);

	    md_com_close(_connection_id);

	    os_thread_mutex_lock(&connection_mutex);
	}
	else
	{
	    server_connection_id = _connection_id;
	}
    }

    if (client_connection_id == -1)
    {
	int _server_connection_id = server_connection_id;

	os_thread_mutex_unlock(&connection_mutex);

	ret = md_com_accept(_server_connection_id, &_connection_id);
	if (ret != COM_SUCCESS)
	{
	    if (ret == COM_CONNECTION_CLOSED)
		exalog_debug("exa_md accept socket was closed.");
	    else
		exalog_error("exa_md cannot accept client connection.");
	    return;
	}

	os_thread_mutex_lock(&connection_mutex);

	if (client_connection_id != -1)
	{
	    os_thread_mutex_unlock(&connection_mutex);
	    md_com_close(_connection_id);
	    os_thread_mutex_lock(&connection_mutex);
	}
	else
	{
	    client_connection_id = _connection_id;
	}
    }

    os_thread_mutex_unlock(&connection_mutex);
}

bool md_srv_com_is_agentx_alive(void)
{
    struct timespec now;
    time_t elapsed;
    os_get_monotonic_time(&now);
    os_thread_mutex_lock(&alive_msg_time_mutex);
    elapsed = difftime(now.tv_sec, alive_msg_time.tv_sec);
    os_thread_mutex_unlock(&alive_msg_time_mutex);
    return elapsed < MD_HEARTBEAT_TIMEOUT_SECONDS;
}

static void md_srv_com_loop(void)
{
    int ret;
    md_com_msg_t *rx_msg;

    check_connection();
    rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
    assert(rx_msg != NULL);

    ret = md_com_recv_msg(client_connection_id, rx_msg);

    if (ret != COM_SUCCESS)
    {
	md_com_msg_free_message(rx_msg);
	close_connection();
	return;
    }

    switch(rx_msg->type)
    {
    case MD_MSG_AGENT_REQ:
	handle_req_msg(rx_msg);
	break;
    case MD_MSG_AGENT_ALIVE:
	handle_alive_msg(rx_msg);
	break;
    default:
	handle_unknown_msg(rx_msg);
    }
    md_com_msg_free_message(rx_msg);

}

void md_srv_com_thread_stop(void)
{
    stop = true;
    os_thread_mutex_lock(&connection_mutex);
    /* The close is necessary to unblock the thread that may be
     * blocked on a accept() call */
    /* FIXME there is a race if another thread tries to send a trap just
     * after this call was done...
     * Anyway, I do not find any clean solution unless reworking the whole
     * stuff; the locking is completly broken in here, races everywhere. */
    md_com_close(server_connection_id);
    server_connection_id = -1;
    md_com_close(client_connection_id);
    client_connection_id = -1;
    os_thread_mutex_unlock(&connection_mutex);
}

void md_srv_com_thread(void *dummy)
{
    while (!stop)
    {
	md_srv_com_loop();
    }
}

void md_srv_com_send_msg(const md_com_msg_t* tx_msg)
{
    int com_ret = COM_UNKNOWN_ERROR;

    while (com_ret != COM_SUCCESS)
    {
	/* this function can block waiting for agentx to reconnect! */
	check_connection();

	/* send the message */
	os_thread_mutex_lock(&connection_mutex);
	com_ret = md_com_send_msg(client_connection_id, tx_msg);
	os_thread_mutex_unlock(&connection_mutex);
	if (com_ret != COM_SUCCESS)
	    close_connection();
    }
}

void md_srv_com_static_init(void)
{
    server_connection_id = -1;
    client_connection_id = -1;
    os_thread_mutex_init(&connection_mutex);
    os_thread_mutex_init(&alive_msg_time_mutex);
}

void md_srv_com_static_clean(void)
{
    server_connection_id = -1;
    client_connection_id = -1;
    os_thread_mutex_destroy(&connection_mutex);
    os_thread_mutex_destroy(&alive_msg_time_mutex);
}


