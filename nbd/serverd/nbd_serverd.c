/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "nbd/serverd/nbd_serverd_perf.h"
#include "nbd/serverd/nbd_serverd.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "admind/include/evmgr_pub_events.h"
#include "common/include/threadonize.h"
#include "common/include/daemon_api_server.h"
#include "common/include/daemon_request_queue.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_perf_instance.h" /* for 'exa_perf_instance_get' */
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "nbd/serverd/nbd_disk_thread.h"
#include "nbd/serverd/ndevs.h"
#include "nbd/service/include/nbd_msg.h"
#include "nbd/common/nbd_tcp.h"
#include "rdev/include/exa_rdev.h"
#include "os/include/os_getopt.h"
#include "os/include/os_thread.h"
#include "os/include/os_file.h"
#include "os/include/os_error.h"
#include "os/include/os_daemon_child.h"
#include "os/include/os_mem.h"

/* The max_receivable_headers corresponds to the max number of request headers
 * that a server can receive. It must be greater than (the max number of
 * requests from the clients' side  x the max number of nodes).
 * The default value of 3000 corresponds to a number of nodes of 40 and a number
 * of 75 requests
 */
static int max_receivable_headers = 300;

server_t nbd_server;

/* callback function from plugin to release the buffer */
static void tcp_server_end_sending(void *ctx, int error)
{
    header_t *req = ctx;

    EXA_ASSERT(req->type == NBD_HEADER_RH);

    if (req->io.buf != NULL)
        nbd_list_post(&nbd_server.ti_queue.free, req->io.buf, -1);

    serverd_perf_end_request(&req->serv_perf);

    nbd_list_post(&nbd_server.list_root.free, req, -1);
}

static void nbd_server_send(exa_nodeid_t to, header_t *req)
{
    /* Send data only if request was a read AND was successful */
    bool send_data = req->io.desc.request_type == NBD_REQ_TYPE_READ
                     && req->io.desc.result == 0;

    EXA_ASSERT(req->type == NBD_HEADER_RH);

    if (!send_data)
    {
        nbd_list_post(&nbd_server.ti_queue.free, req->io.buf, -1);
        req->io.buf            = NULL;
        req->io.desc.sector_nb = 0;
    }

    tcp_send_data(nbd_server.tcp, to, &req->io.desc, sizeof(req->io.desc),
                  req->io.buf, SECTORS_TO_BYTES(req->io.desc.sector_nb),
                  req);
}

void nbd_server_end_io(header_t *req)
{
    nbd_server_send(req->from, req);
}

static bool nbd_recv_processing(exa_nodeid_t from, const nbd_io_desc_t *io, void **data)
{
    if (io->request_type == NBD_REQ_TYPE_WRITE && *data == NULL)
    {
        *data = nbd_list_remove(&nbd_server.ti_queue.free, NULL, LISTNOWAIT);
        return true;
    }
    else
    {
        header_t *req_header = nbd_list_remove(&nbd_server.list_root.free, NULL, LISTNOWAIT);

        EXA_ASSERT(req_header != NULL); /* As many header as receive buff */

        req_header->from = from;

        req_header->io.desc = *io;
        req_header->type = NBD_HEADER_RH;

        /* FIXME Knowing that operation is a read or write seems really useless
         * for perfs here... this should be done by rdev perfs... */
        serverd_perf_make_request(&req_header->serv_perf,
                                  io->request_type == NBD_REQ_TYPE_READ,
                                  io->sector, io->sector_nb);

        /* put directly the header on the appropriate disk queue (the first
         * approach was to put this header on the control blocs queue for
         * the TI thread) */
        os_thread_mutex_lock(&nbd_server.mutex_edevs);
        if (nbd_server.devices[io->disk_id] != NULL)
        {
            if (*data == NULL)
                *data = nbd_list_remove(&nbd_server.ti_queue.free, NULL, LISTNOWAIT);

            req_header->io.buf = *data;
            EXA_ASSERT(req_header->io.buf != NULL);

            req_header->io.desc.result = -EINPROGRESS;
            nbd_list_post(&nbd_server.devices[io->disk_id]->disk_queue, req_header, -1);
        }
        else
        {
            /* the disk no more exist, so we send an error to the sender
             * this send is needed by the sender and by the plugin (ibverbs) to
             * clear some allocated resources */
            req_header->io.desc.result = -EIO;
            nbd_server_send(from, req_header);
        }
        os_thread_mutex_unlock(&nbd_server.mutex_edevs);
    }
    return false;
}

static void server_handle_events(void *p);

/* Function to load a server plugin and do all needed stuff */
static int init_tcp_server(const char *net_type)
{
    int err;

    EXA_ASSERT(nbd_server.tcp == NULL);
    nbd_server.tcp = os_malloc(sizeof(nbd_tcp_t));
    if (nbd_server.tcp == NULL)
        return -ENOMEM;

    nbd_server.tcp->end_sending = tcp_server_end_sending;
    nbd_server.tcp->keep_receiving = nbd_recv_processing;

    /* we will not export our buffer */
    err = init_tcp(nbd_server.tcp, nbd_server.node_name, net_type,
                   nbd_server.num_receive_headers);

    if (err == EXA_SUCCESS)
        err = tcp_start_listening(nbd_server.tcp);

    if (err != EXA_SUCCESS)
	exalog_error("serverd : plugin %s opening error %d", net_type, err);

    return err;
}

static void clean_serverd(void)
{
    os_thread_mutex_destroy(&nbd_server.mutex_edevs);

    exa_rdev_static_clean(RDEV_STATIC_RELEASE);

    os_sem_destroy(&nbd_server.mailbox_sem);
    examsgDelMbox(nbd_server.mh, EXAMSG_NBD_SERVER_ID);
    examsgExit(nbd_server.mh);
}

static int init_serverd(char *net_type)
{
    int retval;

    nbd_server.run = true;

    nbd_server.tcp = NULL;

    nbd_server.num_receive_headers = max_receivable_headers;

    /* First : initialising shared queue */
    nbd_init_root(nbd_server.num_receive_headers, sizeof(header_t),
		  &nbd_server.list_root);
    nbd_init_root(nbd_server.num_receive_headers, /* as many buffer as headers. */
		  nbd_server.bd_buffer_size, &nbd_server.ti_queue);

    os_thread_mutex_init(&nbd_server.mutex_edevs);

    /* FIXME memset is NOT a correct initialization  (especially for pointers...) */
    memset((void *)nbd_server.devices, 0, NBMAX_DISKS_PER_NODE * sizeof(void *));

    os_sem_init(&nbd_server.mailbox_sem ,0);

    /* Initialize the rdev module */
    retval = exa_rdev_static_init(RDEV_STATIC_GET);
    if (retval != 0)
    {
        exalog_error("exa_rdev_static_init() failed: %s", exa_error_msg(retval));
        return -NBD_ERR_SERVERD_INIT;
    }

    nbd_server.exa_rdev_fd = exa_rdev_init();
    if (nbd_server.exa_rdev_fd <= 0)
    {
	exalog_error("Can not open the exa_rdev char device driver file ");
	return -NBD_ERR_SERVERD_INIT;
    }

    if (net_type != NULL)
	retval = init_tcp_server(net_type);
    else
        return -NBD_ERR_MOD_SESSION;

    if (retval != EXA_SUCCESS)
	return retval;

    /* launch server side threads : TD, TI, TR and TTS */

    /* create request queue for communication between the daemon and the
     * working thread
     */
    nbd_server.server_requests_queue =
	daemon_request_queue_new("nbd_server_queue");
    EXA_ASSERT(nbd_server.server_requests_queue);

    /* launch the events handling thread */
    if (!exathread_create(&nbd_server.teh_pid,
			  NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
			  server_handle_events, NULL))
    {
	exalog_debug("failed to launch the events handling thread ");
	return -NBD_ERR_THREAD_CREATION;
    }

    /* launch the rebuild helper thread */
    if (!exathread_create_named(&nbd_server.rebuild_helper_id,
				NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
				rebuild_helper_thread, NULL,"nbd_rebuild"))
    {
	exalog_error("rebuild helper thread creation failed ");
	return -NBD_ERR_THREAD_CREATION;
    }

    os_sem_wait(&nbd_server.mailbox_sem);

    /* initialize examsg framework */
    nbd_server.mh = examsgInit(EXAMSG_NBD_SERVER_ID);
    EXA_ASSERT (nbd_server.mh != NULL);

    /* create local mailbox, buffer at most EXAMSG_MSG_MAX messages */
    exalog_debug("creating mailbox");
    retval = examsgAddMbox(nbd_server.mh,
			   EXAMSG_NBD_SERVER_ID, 1, EXAMSG_MSG_MAX);
    EXA_ASSERT (retval == 0);

    exalog_debug("Rebuild helper thread launched ");

    return EXA_SUCCESS;
}


static int stop_threads(void)
{
    device_t *dev;
    int i;

    /* close disk threads and free associated resources */
    for (i = 0; i < NBMAX_DISKS_PER_NODE; i++)
    {
	dev = nbd_server.devices[i];
	if (dev != NULL)
	{
	    if (dev->handle != NULL)
		exa_rdev_handle_free(dev->handle);
	    os_free(dev);
	    nbd_server.devices[i] = NULL;
	}
    }

    os_thread_join(nbd_server.rebuild_helper_id);

    tcp_stop_listening(nbd_server.tcp);

    /* close net plugin */
    cleanup_tcp(nbd_server.tcp);

    os_free(nbd_server.tcp);
    nbd_server.tcp = NULL;

    return EXA_SUCCESS;
}

static void server_handle_events(void *p)
{
    int retval;
    nbd_request_t req;
    nbd_answer_t ans;
    ExamsgID from;

    exalog_as(EXAMSG_NBD_SERVER_ID);

    while(nbd_server.run)
    {
	/* wait for a new request to handle */
	daemon_request_queue_get_request(nbd_server.server_requests_queue,
					 &req, sizeof(req), &from);

	switch(req.event)
	{
	case NBDCMD_DEVICE_EXPORT:
	    retval = export_device(&req.device_uuid, req.device_path);
	    break;

	case NBDCMD_DEVICE_UNEXPORT:
	    retval = unexport_device(&req.device_uuid);
	    break;

	case NBDCMD_ADD_CLIENT:
	    retval = server_add_client(req.node_name, req.net_id, req.node_id);
	    break;

	case NBDCMD_REMOVE_CLIENT:
	    retval = server_remove_client(req.node_id);
	    break;

	case NBDCMD_QUIT:
	    /* message to close all threads and quit */
	    retval = EXA_SUCCESS;
	    break;

	default :
	{
	    exalog_error("Unknown supervisor event %d", req.event);
	    retval = -EINVAL;
	}
	}

	ans.status = retval;
	EXA_ASSERT(daemon_request_queue_reply(nbd_server.mh, from,
					      nbd_server.server_requests_queue,
					      &ans, sizeof(nbd_answer_t)) == 0);
	if (req.event == NBDCMD_QUIT)
	{
	    nbd_server.run = false;
	    nbd_close_root(&nbd_server.list_root);
	    nbd_close_root(&nbd_server.ti_queue);
	    retval = stop_threads();
	    close(nbd_server.exa_rdev_fd);
	    return;
	}
    }
}

static void handle_daemon_req_msg(const nbd_request_t *req, ExamsgID from)
{
    exalog_debug("serverd event %d", req->event);

    EXA_ASSERT(NBDCMD_IS_VALID(req->event));

    switch (req->event)
    {
    case NBDCMD_QUIT:
	nbd_server.run = false;
	/* Careful no break, fallthru */
    case NBDCMD_ADD_CLIENT:
    case NBDCMD_REMOVE_CLIENT:
    case NBDCMD_DEVICE_EXPORT:
    case NBDCMD_DEVICE_UNEXPORT:
	daemon_request_queue_add_request(nbd_server.server_requests_queue,
					 req, sizeof(nbd_request_t), from);
	break;

    case NBDCMD_NDEV_INFO:
	nbd_ndev_getinfo(&req->device_uuid, from);
	break;

    case NBDCMD_DEVICE_DOWN:
    case NBDCMD_DEVICE_SUSPEND:
    case NBDCMD_DEVICE_RESUME:
    case NBDCMD_DEVICE_IMPORT:
    case NBDCMD_DEVICE_REMOVE:
    case NBDCMD_SESSION_OPEN:
    case NBDCMD_SESSION_CLOSE:
    case NBDCMD_STATS:
	EXA_ASSERT_VERBOSE(false, "Serverd is not supposed to"
			   " receive messages of type %d", req->event);
	break;
    }
}

static int server_events_loop(void)
{
#define TIMEOUT ((struct timeval){ .tv_sec = 1, .tv_usec = 0 })
    struct timeval timeout = TIMEOUT;
    int retval;
    Examsg msg;
    ExamsgMID from;

    while (nbd_server.run)
    {
	retval = examsgWaitTimeout(nbd_server.mh, &timeout);

	if (retval == -ETIME)
	{
	    timeout = TIMEOUT;
	    continue;
	}

	if (retval != 0)
	{
	    exalog_debug("error %d during event loop", retval);
	    break;
	}


	retval = examsgRecv(nbd_server.mh, &from, &msg, sizeof(msg));

	/* Error while message reception */
	if (retval < 0)
	{
	    exalog_debug("error %d during event loop", retval);
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
	    /* handle interrupt */
	    daemon_request_queue_add_interrupt(nbd_server.server_requests_queue,
					       nbd_server.mh, from.id);
	    break;

	default:
	    /* unknown message type */
	    EXA_ASSERT_VERBOSE (false, " got unknown message: `%d'",
				msg.any.type);
	    break;
	}
    }

    os_thread_join(nbd_server.teh_pid);

    if (nbd_server.server_requests_queue)
	daemon_request_queue_delete(nbd_server.server_requests_queue);

    return EXA_SUCCESS;
}

int daemon_init(int argc, char *argv[])
{
    int opt;
    char *net_type = NULL;
    int retval;

    retval = examsg_static_init(EXAMSG_STATIC_GET);
    if (retval != EXA_SUCCESS)
	return retval;

    /* MUST be called BEFORE spawning threads */
    exalog_static_init();

    retval = exa_perf_instance_static_init();
    if (retval != EXA_SUCCESS)
    {
        exalog_error("Failed to initialize exaperf library: %s", exa_error_msg(retval));
        return retval;
    }

    exalog_as(EXAMSG_NBD_SERVER_ID);

    nbd_server.node_name = NULL;
    nbd_server.node_id = EXA_NODEID_NONE;
    nbd_server.bd_buffer_size = DEFAULT_BD_BUFFER_SIZE;

    while ((opt = os_getopt(argc, argv, "n:h:i:s:b:c:d:m:lf")) != -1)
    {
	switch (opt)
	{
	    /* network type */
	    /* to set the TCP buffers size to 128 K, the net_type must be
	     * of the form: TCP=128
	     */
	case 'n':
	    net_type = optarg;
	    break;

	    /* host name */
	case 'h':
	    nbd_server.node_name = optarg;
	    break;

	    /* host id */
        case 'i':
            if (exa_nodeid_from_str(&nbd_server.node_id, optarg) != EXA_SUCCESS)
            {
                exalog_error("Invalid node id");
                /* XXX Is this the right error code? */
                return -EXA_ERR_INVALID_PARAM;
            }
	    break;

	    /* requests size */
	case 'b':
            if (to_int(optarg, &nbd_server.bd_buffer_size) != EXA_SUCCESS)
            {
                exalog_error("Invalid buffer size");
                return -EXA_ERR_INVALID_PARAM;
            }
            /* XXX Should not do that... or at least, not silently */
	    if ((nbd_server.bd_buffer_size & 4095) != 0)
		nbd_server.bd_buffer_size = DEFAULT_BD_BUFFER_SIZE;
	    break;

	    /* number of communication buffers */
	case 'c':
            /* Ignored, kept for compatibility */
	    break;

	    /* the maximum number of headers received by the server */
	case 'm':
            if (to_int(optarg, &max_receivable_headers) != EXA_SUCCESS)
            {
                exalog_error("Invalid max number of receivable headers");
                return -EXA_ERR_INVALID_PARAM;
            }
	    break;

	    /* datanetwork failure detection timeout */
	case 'd':
            /* Ignored kept for compat */
	    break;

 	default:
	    exalog_error("-EXA_ERR_SERVICE_PARAM_UNKNOWN");
	    return -EXA_ERR_SERVICE_PARAM_UNKNOWN;
	}
    }

    retval = init_serverd(net_type);
    if (retval != EXA_SUCCESS)
	return retval;

    return EXA_SUCCESS;
}

int daemon_main(void)
{
    /* wait for commands and handle them */
    server_events_loop();

    clean_serverd();

    exa_perf_instance_static_clean();

    exalog_static_clean();

    examsg_static_clean(EXAMSG_STATIC_RELEASE);

    os_meminfo("Serverd", OS_MEMINFO_DETAILED);

    return EXA_SUCCESS;
}



