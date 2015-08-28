/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "common/include/exa_error.h"
#include "common/include/daemon_api_server.h"
#include "common/include/daemon_request_queue.h"
#include "os/include/os_thread.h"
#include "os/include/os_error.h"
#include "os/include/os_string.h"

#include "lum/executive/include/lum_executive.h"
#include "lum/executive/src/work_thread.h"

ExamsgHandle lum_mh;

static bool stop = false;

static os_thread_t lum_main_thread;

/**
 * Get the right queue for the message
 *
 * @return void
 */
static struct daemon_request_queue *lum_get_queue(ExamsgID from)
{
    switch (from)
    {
    case EXAMSG_ADMIND_INFO_LOCAL:
        return lum_request_info_queue;

        /* No special queue for recovery as the lum service is above VRT
         * and thus it should be never be bloked when performing a command
         * (The command may take time but MUST complete) */
    case EXAMSG_ADMIND_CMD_LOCAL:
    case EXAMSG_ADMIND_RECOVERY_LOCAL:
        return lum_request_command_queue;

    default:
        EXA_ASSERT_VERBOSE(false, "Unexpected message from '%s'(%d)",
                           examsgIdToName(from), from);
        return NULL;
    }
}

/**
 * The event loop of the exa_lum daemon.
 */
static void lum_process_loop(void)
{
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    Examsg msg;
    ExamsgMID from;
    int retval;
    struct daemon_request_queue *queue;

    /* wait for a message or a signal */
    retval = examsgWaitInterruptible(lum_mh, &timeout);
    if (retval == -EINTR || retval == -ETIME)
        return;

    EXA_ASSERT(retval == 0);

    memset(&msg, 0, sizeof(msg));
    memset(&from, 0, sizeof(from));

    retval = examsgRecv(lum_mh, &from, &msg, sizeof(msg));

    EXA_ASSERT(retval >= 0);

    /* No message */
    if (retval == 0)
        return;

    queue = lum_get_queue(from.id);

    if (msg.any.type == EXAMSG_DAEMON_RQST)
    {
        EXA_ASSERT(retval > sizeof(msg.any));
        daemon_request_queue_add_request(queue, msg.payload,
            retval - sizeof(msg.any), from.id);
    }
    else if (msg.any.type == EXAMSG_DAEMON_INTERRUPT)
        daemon_request_queue_add_interrupt(queue, lum_mh, from.id);
    else
        EXA_ASSERT_VERBOSE(false, "Cannot handle this type of message: %d",
               msg.any.type);
}

static int lum_daemon_init(void)
{
    int ret;

    exalog_as(EXAMSG_LUM_ID);

    lum_mh = examsgInit (EXAMSG_LUM_ID);
    EXA_ASSERT(lum_mh != NULL);

    ret = examsgAddMbox(lum_mh, EXAMSG_LUM_ID, 3, EXAMSG_MSG_MAX);
    EXA_ASSERT(ret == 0);

    lum_workthreads_start();

    return EXA_SUCCESS;
}

static void lum_daemon_cleanup(void)
{
    lum_workthreads_stop();

    examsgDelMbox(lum_mh, EXAMSG_LUM_ID);

    /* Exit examsg */
    examsgExit(lum_mh);
}

static void lum_daemon_main(void *dummy)
{
    while (!stop)
        lum_process_loop();

    lum_daemon_cleanup();
}

/**
 * Create and start the lum executive thread.
 *
 * @return 0 on success or an error code.
 */
int lum_thread_create(void)
{
    int err;

    err = lum_daemon_init();
    if (err != EXA_SUCCESS)
        return err;

    if (os_thread_create(&lum_main_thread, 0, lum_daemon_main, NULL))
        return EXA_SUCCESS;
    else
        return -EXA_ERR_THREAD_CREATE;
}

/**
 * Stop the lum executive thread.
 *
 * @return 0 on success or an error code.
 */
int lum_thread_stop(void)
{
    stop = true;
    os_thread_join(lum_main_thread);
    return EXA_SUCCESS;
}

