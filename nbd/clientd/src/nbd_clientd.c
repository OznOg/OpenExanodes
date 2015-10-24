/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/clientd/src/bd_user_user.h"
#include "nbd/clientd/src/nbd_clientd_perf_private.h"
#include "nbd/clientd/src/nbd_clientd_private.h"
#include "nbd/clientd/src/nbd_stats.h"

#include "nbd/clientd/include/nbd_clientd.h"

#include "nbd/service/include/nbd_msg.h"

#include "nbd/common/nbd_tcp.h"

#include "vrt/virtualiseur/include/vrt_init.h"

#include "lum/export/include/executive_export.h"
#include "lum/executive/include/lum_executive.h"

#include "common/include/exa_config.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_names.h"
#include "common/include/exa_perf_instance.h"
#include "common/include/daemon_api_server.h"
#include "common/include/daemon_request_queue.h"
#include "common/include/threadonize.h"

#include "examsg/include/examsg.h"

#include "os/include/os_daemon_child.h"
#include "os/include/os_getopt.h"
#include "os/include/os_mem.h"
#include "os/include/os_random.h"

#include <errno.h>

#ifndef WIN32
#include <signal.h>
#endif

static struct daemon_request_queue *client_requests_queue = NULL;

/* Examsg mail box */
static ExamsgHandle clientd_mh;

/* FIXME the was to stop clientd seems really crappy... this boolean seems
 * to be modified by every thread with the hope that something eventually
 * happen... */
static volatile bool clientd_run;
static os_thread_t event_thread_tid;

static nbd_tcp_t tcp;

#define NUM_SLOWDOWN_VALUES  5
static int slowdown_values_ms[NUM_SLOWDOWN_VALUES] = { 0, 25, 50, 100, 200 };

static void client_handle_events(void *p);

static int client_open_session(nbd_tcp_t *nbd_tcp, const char *node_name,
                               const char *ip_addr, exa_nodeid_t node_id)
{
#define MAX_TRY_CONNECT 3
    int try_count;
    int err = tcp_add_peer(node_id, ip_addr, nbd_tcp);

    if (err != EXA_SUCCESS)
        return err;

    /* Now peer was added, the client tries to connect to servers */

    /* we try MAX_TRY_CONNECT times to connect because on huge clusters, there
     * may be contention at this point because all nodes may try to get
     * connected at the same time. */
    for (try_count = MAX_TRY_CONNECT; try_count > 0; try_count--)
    {
        err = tcp_connect_to_peer(nbd_tcp, node_id);
        if (err == EXA_SUCCESS)
            break;

        exalog_error("keep trying %d time(s)", try_count);
	os_sleep(1);
    }

    return err;
}

static int client_close_session(nbd_tcp_t *nbd_tcp, char *node_name,
                                exa_nodeid_t node_id)
{
    /* FIXME check return value */
    tcp_remove_peer(node_id, nbd_tcp);

    return EXA_SUCCESS;
}

blockdevice_t *client_get_blockdevice(const exa_uuid_t *uuid)
{
    return exa_bdget_block_device(uuid);
}

/**
 * FIXME this function is mainly here to 'hide' tcp to other files.
 * This enforces encapsulation, but I dislike this kind of artificial
 * function with no real symetric equivalent. Encapsulation seems broken
 * this should be reworked. */
void header_sending(exa_nodeid_t to, const nbd_io_desc_t *io)
{
    tcp_send_data(&tcp, to, io);
}

static void end_receiving(exa_nodeid_t from, const nbd_io_desc_t *io, int error)
{
    exa_bd_end_request(io);
}

static void *client_get_buffer(const nbd_io_desc_t *io)
{
    return exa_bdget_buffer(io->req_num);
}

#ifndef WIN32
static void signal_handler(int sig)
{
    /* XXX
     * For some unknown reason that I can't figure out, this utterly
     * useless signal handler presence's seems required to be able to
     * interrupt exa_clientd in gdb. This is absolutely strange and
     * illogic.
     */
}
#endif

static int init_clientd(const char *net_type, const char *hostname,
                        bool barrier_enable, int max_req_num, int buffer_size)
{
    int retval;
    int num_receive_headers;

    /* As we can receive request burst, there may be max_req_num
     * outstanding requests thus num_receive_headers are needed in the
     * worst case. You may notice that when the request was sent, the
     * header is freed but not the request itself. This means that the
     * number of header does not really need to be huge, but putting a
     * smaller value could lead to contention upon send.
     * FIXME have a real study of how many header are really needed in
     * proportion. */
    num_receive_headers = max_req_num;

    if (!exa_bdinit(buffer_size, max_req_num, barrier_enable))
    {
	exalog_error("Cannot create session with Bd");
	return -NBD_ERR_MOD_SESSION;
    }

    if (net_type == NULL)
        return -NBD_ERR_MOD_SESSION;

    tcp.get_buffer = client_get_buffer;

    /* no need of end_sending because the buffer will be realesed when
     * we get the next message from server 'IOD", and so after the
     * successfull sending of this messages */
    tcp.end_sending = NULL;
    tcp.end_receiving = end_receiving;

    retval = init_tcp(&tcp, hostname, net_type, num_receive_headers);
    if (retval != EXA_SUCCESS)
    {
	exalog_error("NBD clientd: failed initializing network '%s': %s (%d)",
                     net_type, exa_error_msg(retval), retval);
	return retval;
    }

    exalog_debug("network init done");

    /* create request queue for communication between the daemon and
     * the working thread
     */
    client_requests_queue = daemon_request_queue_new("nbd_client_queue");
    EXA_ASSERT(client_requests_queue);

    /* MUST be set before starting any of the threads below */
    clientd_run = true;

    /* launch the event handling thread */
    if (!exathread_create(&event_thread_tid,
			  NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
			  client_handle_events, NULL))
    {
	exalog_error("Failed to launch the event handling thread");
	return -NBD_ERR_THREAD_CREATION;
    }

    /* initialize examsg framework */
    clientd_mh = examsgInit(EXAMSG_NBD_CLIENT_ID);
    EXA_ASSERT(clientd_mh != NULL);

    /* create local mailbox, buffer at most EXAMSG_MSG_MAX messages */
    exalog_debug("creating mailbox");
    retval = examsgAddMbox(clientd_mh,
			   EXAMSG_NBD_CLIENT_ID, 1, EXAMSG_MSG_MAX);
    EXA_ASSERT(retval == 0);

    exalog_debug("client mailbox created");

#ifndef WIN32
    signal(SIGUSR1, signal_handler);
#endif

    return 0;
}

/* FIXME This does more than just stopping threads */
static int stop_threads(void)
{
    cleanup_tcp(&tcp);

    exa_bdend();

    return EXA_SUCCESS;
}

static int cleanup_clientd(void)
{
    int err;

    lum_export_static_cleanup();

    vrt_exit();
    lum_thread_stop();

    err = stop_threads();

    clientd_perf_cleanup();

    return err;
}

static void client_handle_events(void *p)
{
    nbd_request_t req;
    nbd_answer_t ans;
    ExamsgID from;
    int err;

    exalog_as(EXAMSG_NBD_CLIENT_ID);

    while (clientd_run)
    {
	/* wait for a new request to handle */
	daemon_request_queue_get_request(client_requests_queue,
					 &req, sizeof(req), &from);

	switch(req.event)
	{
	case NBDCMD_SESSION_OPEN:
	    exalog_debug("SESSION_OPEN message received");
	    err = client_open_session(&tcp, req.node_name, req.net_id,
                                      req.node_id);
	    break;

	case NBDCMD_SESSION_CLOSE:
	    exalog_debug("SESSION_CLOSE message received");
	    err = client_close_session(&tcp, req.node_name, req.node_id);
	    break;

	case NBDCMD_DEVICE_IMPORT:
	    exalog_debug("DEVICE_IMPORT message received");
	    err = client_import_device(&req.device_uuid, req.node_id,
                                       req.device_sectors, req.device_nb);
	    break;

	case NBDCMD_DEVICE_SUSPEND:
	    exalog_debug("DEVICE_SUSPEND message received");
	    err = client_suspend_device(&req.device_uuid);
	    break;

	case NBDCMD_DEVICE_DOWN:
	    exalog_debug("DEVICE_DOWN message received");
	    err = client_down_device(&req.device_uuid);
	    break;

	case NBDCMD_DEVICE_RESUME:
	    exalog_debug("DEVICE_RESUME message received ");
	    err = client_resume_device(&req.device_uuid);
	    break;

	case NBDCMD_DEVICE_REMOVE:
	    exalog_debug("DEVICE_REMOVE message received");
	    err = client_remove_device(&req.device_uuid);
	    break;

	case NBDCMD_QUIT:
	    exalog_debug("QUIT message received");
	    clientd_run = false;
	    err = cleanup_clientd();
	    break;

	default:
	    exalog_error("Unknown supervisor event %d", req.event);
	    err = -EINVAL;
	}

	ans.status = err;
	EXA_ASSERT(daemon_request_queue_reply(clientd_mh, from,
					      client_requests_queue,
					      &ans, sizeof(nbd_answer_t)) == 0);
    }
}

static void handle_daemon_req_msg(const nbd_request_t *req, ExamsgID from)
{
    exalog_debug("clientd event %d", req->event);

    EXA_ASSERT(NBDCMD_IS_VALID(req->event));

    switch (req->event)
    {
    case NBDCMD_QUIT:
	clientd_run = false;
	/* Careful no break */

    case NBDCMD_SESSION_OPEN:
    case NBDCMD_SESSION_CLOSE:
    case NBDCMD_DEVICE_IMPORT:
    case NBDCMD_DEVICE_SUSPEND:
    case NBDCMD_DEVICE_DOWN:
    case NBDCMD_DEVICE_RESUME:
    case NBDCMD_DEVICE_REMOVE:
	daemon_request_queue_add_request(client_requests_queue,
					 req, sizeof(*req), from);
	break;

    case NBDCMD_STATS:
        {
            struct nbd_stats_reply reply;
            memset(&reply, 0, sizeof(reply));

            bd_get_stats(&reply, &req->device_uuid, req->stats_reset);

            admwrk_daemon_reply(clientd_mh, from, &reply, sizeof(reply));
        }
	break;

    case NBDCMD_DEVICE_EXPORT:
    case NBDCMD_DEVICE_UNEXPORT:
    case NBDCMD_ADD_CLIENT:
    case NBDCMD_REMOVE_CLIENT:
    case NBDCMD_NDEV_INFO:
	EXA_ASSERT_VERBOSE(false, "Clientd does not"
			   " handle event type %d", req->event);
	break;
    }

}

static int client_event_loop(void)
{
    int retval;
    Examsg msg;
    ExamsgMID from;

    exalog_debug("clientd examsg events loop waiting for messages");

    while (clientd_run)
    {
	retval = examsgWait(clientd_mh);

	if (retval != 0)
	{
	    exalog_debug("error %d during event loop", retval);
	    break;
	}

	retval = examsgRecv(clientd_mh, &from, &msg, sizeof(msg));
	if (retval < 0)
	{
	    exalog_debug("failed receiving examsg: error %d", retval);
	    break;
	}

	/* No message */
	if (retval == 0)
	    continue;

	switch (msg.any.type)
	{
	case EXAMSG_DAEMON_RQST:
	    EXA_ASSERT(retval == sizeof(nbd_request_t) + sizeof(msg.any));
	    handle_daemon_req_msg((nbd_request_t *)msg.payload, from.id);
	    break;

	case EXAMSG_DAEMON_INTERRUPT:
	    exalog_debug("EXAMSG_DAEMON_INTERRUPT message received");
	    daemon_request_queue_add_interrupt(client_requests_queue,
		                               clientd_mh, from.id);
	    break;

	default:
	    EXA_ASSERT_VERBOSE(false, "Clientd does not handle"
		               " messages type %d", msg.any.type);
	    break;
	}
    }

    os_thread_join(event_thread_tid);

    if (clientd_mh)
    {
	examsgDelMbox(clientd_mh, EXAMSG_NBD_CLIENT_ID);
	examsgExit(clientd_mh);
    }

    if (client_requests_queue)
	daemon_request_queue_delete(client_requests_queue);
    client_requests_queue = NULL;

    return EXA_SUCCESS;
}

static int get_slowdown(const char *str, int *slowdown_ms)
{
    unsigned int index;

    if (to_uint(str, &index) != EXA_SUCCESS)
        return -EINVAL;

    if (index >= NUM_SLOWDOWN_VALUES)
        return -ERANGE;

    *slowdown_ms = slowdown_values_ms[index];

    return EXA_SUCCESS;
}

int daemon_init(int argc, char *argv[])
{
    int retval;
    int opt;
    exa_nodeid_t my_node_id = EXA_NODEID_NONE;
    int vrt_max_requests = 0;
    int vrt_rebuilding_slowdown_ms = -1;
    int vrt_degraded_rebuilding_slowdown_ms = -1;
    char *net_type = NULL;
    char *node_name = NULL;
    bool barrier_enable = true;
    int bd_buffer_size = DEFAULT_BD_BUFFER_SIZE;
    int max_req_num    = DEFAULT_MAX_CLIENT_REQUESTS;

    while ((opt = os_getopt(argc, argv, "B:c:n:h:s:S:t:b:p:l:A:M:")) != -1)
    {
        switch (opt)
        {
        case 'A':
            if (exa_nodeid_from_str(&my_node_id, optarg) != EXA_SUCCESS)
            {
                exalog_error("Invalid node id");
                return -EXA_ERR_INVALID_PARAM;
            }
            break;

        case 'M':
            if (to_int(optarg, &vrt_max_requests) != EXA_SUCCESS)
            {
                exalog_error("Invalid maximum number of requests");
                return -EXA_ERR_INVALID_PARAM;
            }
            break;

        case 'B':
            if (strcmp(optarg, "TRUE") == 0)
                barrier_enable = true;
            else
                barrier_enable = false;
            break;

        /* datanetwork failure detection timeout */
        case 'c':
            /* Ignored kept for compat */
            break;

        /* network type
         * to set the TCP buffers size to 128 K, the net_type must be
         * of the form: TCP=128
         */
        case 'n':
            net_type = optarg;
            break;

        /* host name */
        case 'h':
            node_name = optarg;
            break;

        /* requests size */
        case 'b':
            if (to_int(optarg, &bd_buffer_size) != EXA_SUCCESS)
            {
                fprintf(stderr, "Invalid buffer size");
                return EXIT_FAILURE;
            }
            if ((bd_buffer_size & 4095) != 0
                || bd_buffer_size < 4096)
            {
                fprintf(stderr, "bd_buffer_size %d not multiple of 4096 or too small, so we will use %d\n",
                        bd_buffer_size, DEFAULT_BD_BUFFER_SIZE);
                bd_buffer_size = DEFAULT_BD_BUFFER_SIZE;
            }
            break;

        /* number of requests */
        case 'p':
            if (to_int(optarg, &max_req_num) != EXA_SUCCESS)
            {
                fprintf(stderr, "Invalid max number of requests");
                return EXIT_FAILURE;
            }
            if (max_req_num < 1)
                max_req_num = DEFAULT_MAX_CLIENT_REQUESTS;
            break;

        case 's':
            if (get_slowdown(optarg, &vrt_rebuilding_slowdown_ms) != EXA_SUCCESS)
            {
                fprintf(stderr, "Invalid VRT rebuilding slowdown");
                return EXIT_FAILURE;
            }
            break;

        case 'S':
            if (get_slowdown(optarg, &vrt_degraded_rebuilding_slowdown_ms)
                != EXA_SUCCESS)
            {
                fprintf(stderr, "Invalid VRT degraded rebuilding slowdown");
                return EXIT_FAILURE;
            }
            break;

        default:
            fprintf(stderr, "Invalid parameter %c\n", opt);
        }
    }

    os_random_init();

    retval = examsg_static_init(EXAMSG_STATIC_GET);
    if (retval != EXA_SUCCESS)
	return retval;

    /* MUST be call BEFORE spawning threads */
    exalog_static_init();

    exalog_as(EXAMSG_NBD_CLIENT_ID);
    exalog_debug("clientd daemonized");

    if (exa_perf_instance_static_init() != 0)
        return -EINVAL; /* FIXME Use better error code */

    retval = init_clientd(net_type, node_name, barrier_enable,
                          max_req_num, bd_buffer_size);
    if (retval != EXA_SUCCESS)
	return retval;

    exalog_debug("clientd global vars init done");

    vrt_init(my_node_id,
             vrt_max_requests,
             barrier_enable,
             vrt_rebuilding_slowdown_ms,
             vrt_degraded_rebuilding_slowdown_ms);

    retval = lum_export_static_init(my_node_id);
    if (retval != EXA_SUCCESS)
    {
        exalog_error("Failed to initialize the LUM executive: %s (%d)",
                     exa_error_msg(retval), retval);
        goto error_export_static_init;
    }

    /* Initialize the LUM executive */
    retval = lum_thread_create();
    if (retval != EXA_SUCCESS)
    {
        exalog_error("Failed to create the LUM executive threads: %s (%d)",
                     exa_error_msg(retval), retval);
        goto error_lum_thread_create;
    }

    return EXA_SUCCESS;

error_export_static_init:
    lum_thread_stop();

error_lum_thread_create:
    if (clientd_mh)
    {
        examsgDelMbox(clientd_mh, EXAMSG_NBD_CLIENT_ID);
        examsgExit(clientd_mh);
    }

    vrt_exit();

    exa_perf_instance_static_clean();
    os_random_cleanup();

    return retval;
}

int daemon_main(void)
{
    client_event_loop();

    exalog_static_clean();
    examsg_static_clean(EXAMSG_STATIC_RELEASE);

    os_meminfo("Clientd", OS_MEMINFO_DETAILED);

    return 0;
}
