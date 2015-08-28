/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "lum/executive/include/lum_executive.h"
#include "lum/executive/src/work_thread.h"

#include "lum/client/src/lum_msg.h"

#include "lum/export//include/executive_export.h"

#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/daemon_request_queue.h"
#include "common/include/threadonize.h"

#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"

#include "target/iscsi/include/target.h"

#define LUM_THREAD_STACK_SIZE 300000

/* Should probably be static, but are currently used in lum_executive.c */
struct daemon_request_queue *lum_request_info_queue;
struct daemon_request_queue *lum_request_command_queue;

static os_thread_t lum_info_thread;
static os_thread_t lum_command_thread;

static bool stop = false;


/**
 * Handle an info request to the exa_lumd daemon by calling the correct
 * funtion.
 *
 * @param[in] queue     Request queue handler.
 * @param[in] req       the request.
 * @param[in] from      Sender of the request
 * @return 0 on success or an error code.
 */
static void lum_handle_info_request(const lum_request_t *req, ExamsgID from)
{
    lum_answer_t answer;

    EXA_ASSERT(req);

    /* crustify answer */
    memset (&answer, 0xEE, sizeof(answer));

    EXA_ASSERT_VERBOSE(LUM_REQUEST_TYPE_IS_VALID(req->type),
                       "Invalid request type %d", req->type);

    switch (req->type)
    {
    case LUM_INFO:
        answer.error = lum_export_get_info(&req->info.export_uuid, &answer.info);
        break;

    case LUM_INFO_GET_NTH_CONNECTED_IQN:
        {
            const lum_info_get_nth_iqn_t *nth_iqn = &req->nth_iqn;
            answer.error = lum_export_get_nth_connected_iqn(&nth_iqn->export_uuid,
                                                        nth_iqn->iqn_num,
                                                        &answer.connected_iqn);
        }
        break;

    case LUM_CMD_SUSPEND:
    case LUM_CMD_RESUME:
    case LUM_CMD_SET_MSHIP:
    case LUM_CMD_SET_PEERS:
    case LUM_CMD_SET_TARGETS:
    case LUM_CMD_EXPORT_PUBLISH:
    case LUM_CMD_EXPORT_UNPUBLISH:
    case LUM_CMD_EXPORT_UPDATE_IQN_FILTERS:
    case LUM_CMD_SET_READAHEAD:
    case LUM_CMD_INIT:
    case LUM_CMD_CLEANUP:
    case LUM_CMD_EXPORT_RESIZE:
    case LUM_CMD_START_TARGET:
    case LUM_CMD_STOP_TARGET:
        EXA_ASSERT_VERBOSE(false, "Wrong request %d for lum info thread",
                           req->type);
        break;
    }

    EXA_ASSERT(daemon_request_queue_reply(lum_mh, from,
               lum_request_info_queue, &answer, sizeof(answer)) == 0);
}

/**
 * Handle a command request to the exa_lumd daemon by calling the correct
 * funtion.
 *
 * @param[in] queue     Request queue handler.
 * @param[in] req       the request.
 * @param[in] from      Sender of the request
 * @return 0 on success or an error code.
 */
static void lum_handle_command_request(const lum_request_t *req, ExamsgID from)
{
    lum_answer_t answer;

    EXA_ASSERT(req);

    /* crustify answer */
    memset(&answer, 0xEE, sizeof(answer));

    EXA_ASSERT_VERBOSE(LUM_REQUEST_TYPE_IS_VALID(req->type),
                       "Invalid request type %d", req->type);

    switch (req->type)
    {
    case LUM_CMD_SUSPEND:
        get_iscsi_adapter()->suspend();
#ifdef WITH_BDEV
        get_bdev_adapter()->suspend();
#endif
        answer.error = EXA_SUCCESS;
        break;

    case LUM_CMD_RESUME:
        get_iscsi_adapter()->resume();
#ifdef WITH_BDEV
        get_bdev_adapter()->resume();
#endif
        answer.error = EXA_SUCCESS;
        break;

    case LUM_CMD_SET_PEERS:
        /* Only iSCSI needs to know peers */
        get_iscsi_adapter()->set_peers(&req->set_peers);
        answer.error = EXA_SUCCESS;
        break;

    case LUM_CMD_SET_TARGETS:
        {
        const lum_target_addresses_t *addrs = &req->target_addresses;
        /* Only iSCSI needs to know targets */
        get_iscsi_adapter()->set_addresses(addrs->num_addr, addrs->addr);
        answer.error = EXA_SUCCESS;
        }
        break;

    case LUM_CMD_SET_MSHIP:
        {
            const exa_nodeset_t *mship = &req->set_mship.mship;

            char mship_str[EXA_MAX_NODES_NUMBER + 1];
            exa_nodeset_to_bin(&req->set_mship.mship, mship_str);

            /* Only iSCSI needs mship; bdev doesn't */
            get_iscsi_adapter()->set_mship(mship);
            answer.error = 0;
        }
        break;

    case LUM_CMD_EXPORT_PUBLISH:
        {
            const lum_cmd_publish_t *pub = &req->publish;
            answer.error = lum_export_export(pub->buf, pub->buf_size);
        }
        break;

    case LUM_CMD_EXPORT_UNPUBLISH:
        {
            const lum_cmd_unpublish_t *unexport = &req->unpublish;
            answer.error = lum_export_unexport(&unexport->export_uuid);
        }
        break;

    case LUM_CMD_EXPORT_UPDATE_IQN_FILTERS:
        {
            const lum_cmd_update_iqn_filters_t *cmd = &req->update_iqn_filters;
            answer.error = lum_export_update_iqn_filters(cmd->buf, cmd->buf_size);
        }
        break;

    case LUM_INFO:
    case LUM_INFO_GET_NTH_CONNECTED_IQN:
        EXA_ASSERT_VERBOSE(false, "Wrong request %d for lum command thread",
                           req->type);
        break;

    case LUM_CMD_SET_READAHEAD:
        {
            const lum_cmd_set_readahead_t *cmd = &req->set_readahead;
            answer.error = lum_export_set_readahead(&cmd->export_uuid,
                                                    cmd->readahead);
        }
        break;

    case LUM_CMD_INIT:
        {
            const lum_cmd_init_t *cmd = &req->init;
            answer.error = get_iscsi_adapter()->init(&cmd->init_params);
#ifdef WITH_BDEV
            if (answer.error == EXA_SUCCESS)
                answer.error = get_bdev_adapter()->init(&cmd->init_params);
#endif
        }
        break;

    case LUM_CMD_CLEANUP:
        {
            answer.error = get_iscsi_adapter()->cleanup();
#ifdef WITH_BDEV
            if (answer.error == EXA_SUCCESS)
                answer.error = get_bdev_adapter()->cleanup();
#endif
        }
        break;

    case LUM_CMD_EXPORT_RESIZE:
        {
            const lum_cmd_export_resize_t *cmd = &req->export_resize;
            answer.error = lum_export_resize(&cmd->export_uuid, cmd->size);
        }
        break;

    case LUM_CMD_START_TARGET:
        answer.error = get_iscsi_adapter()->start_target();
        break;

    case LUM_CMD_STOP_TARGET:
        answer.error = get_iscsi_adapter()->stop_target();
        break;

    }

    EXA_ASSERT(daemon_request_queue_reply(lum_mh, from,
              lum_request_command_queue, &answer, sizeof(answer)) == 0);
}

/**
 * Main info thread function
 *
 * @param[in] arg an argument to pass to the thread - unused
 *
 * @return void
 */
static void lum_info_thread_run(void *arg)
{
    exalog_as(EXAMSG_LUM_ID);

    while (!stop)
    {
        lum_request_t req;
        ExamsgID from;
        int r;

        /* lum_queue_get_request() blocks until a request income */
        r = daemon_request_queue_get_request(lum_request_info_queue, &req,
                                             sizeof(req), &from);
        if (r == 0)
            /* handle the request and send the reply */
            lum_handle_info_request(&req, from);
    }
}

/**
 * Main command thread function
 *
 * @param[in] arg an argument to pass to the thread - unused
 *
 * @return void
 */
static void lum_command_thread_run(void *arg)
{
    exalog_as(EXAMSG_LUM_ID);

    while (!stop)
    {
        /* Have a buffer of the max size that can bear a daemon request
         * because some messages have a variable size (eg lum_export_publish)*/
        static char __lum_msg_buf[DAEMON_REQUEST_MSG_MAXSIZE];
        lum_request_t *req = (lum_request_t *)__lum_msg_buf;
        ExamsgID from;
        int r;

        /* lum_queue_get_request() blocks until a request income */
        r = daemon_request_queue_get_request(lum_request_command_queue, req,
                                             sizeof(__lum_msg_buf), &from);
        if (r == 0)
            /* handle the request and send the reply */
            lum_handle_command_request(req, from);
    }
}

/**
 * Initialize the info and command threads
 *
 * @return void
 */
void lum_workthreads_start(void)
{
    /* Initialize queues */
    lum_request_info_queue = daemon_request_queue_new("lum_info_queue");
    EXA_ASSERT(lum_request_info_queue != NULL);
    lum_request_command_queue = daemon_request_queue_new("lum_request_command_queue");
    EXA_ASSERT(lum_request_command_queue != NULL);

    exathread_create_named (&lum_info_thread,
                            LUM_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
                            &lum_info_thread_run, NULL,
                            "lum_info_thread");

    exathread_create_named (&lum_command_thread,
                            LUM_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
                            &lum_command_thread_run, NULL,
                            "lum_command_thread");
}

/**
 * Stop the info and command threads
 *
 * @return void
 */
void lum_workthreads_stop(void)
{
    stop = true;
    daemon_request_queue_break_get_request(lum_request_info_queue);
    daemon_request_queue_break_get_request(lum_request_command_queue);

    os_thread_join(lum_info_thread);
    os_thread_join(lum_command_thread);

    daemon_request_queue_delete(lum_request_info_queue);
    daemon_request_queue_delete(lum_request_command_queue);
}
