/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "examsg/include/examsg.h"
#include "examsg/src/objpoolapi.h"
#include "log/include/log.h"

#include "os/include/os_stdio.h"
#include "os/include/os_error.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Test whether examsgWait() does busy waiting:
 *
 * (1) A message is deposited in a mailbox
 *
 * (2) A 1st examsgWait() is issued, which should return right away as there
 *     is a new message in the mailbox
 *
 * (3) A 2nd examsgWait() is issued, which should block indefinitely, as the
 *     only message in the mailbox is the one that was already there at the
 *     1st examsgWait() and thus there aren't any new message.
 *     If examsgWait() returns there, it means it would busywait if called
 *     in a loop.
 */
ut_test(examsgWait_does_not_busy_wait) __ut_lengthy
{
    ExamsgHandle mh;
    char buf[1024];
    int err;
    int n;
    /* A 10s timeout is "big enough" */
    struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };

    exalog_as(EXAMSG_TEST_ID);

    err = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(err == 0);

    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);

    n = examsgAddMbox(mh, examsgOwner(mh), 1, sizeof(buf));
    UT_ASSERT_VERBOSE(n == 0, "examsgAddMbox failed: got %d, expected 0", n);

    os_snprintf(buf, sizeof(buf), "busy_busy_busy");
    n = examsgSend(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, buf, strlen(buf) + 1);
    UT_ASSERT_VERBOSE(n == strlen(buf) + 1, "examsgSend: returned %d, expected %"PRIzu,
                      n, strlen(buf) + 1);

    n = examsgWait(mh);
    UT_ASSERT_VERBOSE(n == 0, "1st examsgWait returned %d, expected 0", n);
    ut_printf("1st wait, the function returned as expected");

    /* We test examsgWaitTimeout() instead of examsgWait() so that we don't
     * actually block indefinitely (the UT library can't consider as PASSED a
     * test case that doesn't finish).
     *
     * If we want to test examsgWait(), we can use SIGALRM on Linux and a
     * waitable timer on Windows (maybe not, since the process must be in a
     * waitable state).
     */
    ut_printf("Second wait, message still there but not new, should not return");
    n = examsgWaitTimeout(mh, &timeout);
    UT_ASSERT_VERBOSE(n == -ETIME, "2nd examsgWait didn't time out");

    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_VERBOSE(n == 0, "examsgDelMbox failed: got %d, expected 0", n);

    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);

    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(nominal_send_recv)
{
    ExamsgHandle mh;
    int n;
    Examsg msg, recv;
    ExamsgMID mid;

    /* init */
    exalog_as(EXAMSG_TEST_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);
    n = examsgAddMbox(mh, examsgOwner(mh), 1, sizeof(msg));
    UT_ASSERT_EQUAL(0, n);

    n = examsgSend(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, &msg, sizeof(msg));
    UT_ASSERT_EQUAL(sizeof(msg), n);

    /* received the message just sent. */
    n = examsgRecv(mh, &mid, &recv, sizeof(recv));
    UT_ASSERT_EQUAL(sizeof(recv), n);

    /* try another receive, but noting to read should return 0 */
    n = examsgRecv(mh, &mid, &recv, sizeof(recv));
    UT_ASSERT_EQUAL(0, n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_EQUAL(0, n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(receive_in_too_small_buff)
{
    ExamsgHandle mh;
    int n, recv;
    Examsg msg;
    ExamsgMID mid;

    /* init */
    exalog_as(EXAMSG_TEST_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);
    n = examsgAddMbox(mh, examsgOwner(mh), 1, sizeof(msg));
    UT_ASSERT_EQUAL(0, n);

    n = examsgSend(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, &msg, sizeof(msg));
    UT_ASSERT_EQUAL(sizeof(msg), n);

    n = examsgRecv(mh, &mid, &recv, sizeof(recv));
    UT_ASSERT_EQUAL(-EMSGSIZE, n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_EQUAL(0, n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(send_mailbox_is_full)
{
    ExamsgHandle mh;
    int n;
    Examsg msg;

    /* init */
    exalog_as(EXAMSG_TEST_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);
    n = examsgAddMbox(mh, examsgOwner(mh), 1, sizeof(msg));
    UT_ASSERT_EQUAL(0, n);

    n = examsgSend(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, &msg, sizeof(msg));
    UT_ASSERT_EQUAL(sizeof(msg), n);

    n = examsgSendNoBlock(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, &msg, sizeof(msg));
    UT_ASSERT_EQUAL(-ENOSPC, n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_EQUAL(0, n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(send_message_too_big)
{
    ExamsgHandle mh;
    int n;
    char buf[EXAMSG_MSG_MAX + 1]; /* one byte too big */

    /* init */
    exalog_as(EXAMSG_TEST_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);
    n = examsgAddMbox(mh, examsgOwner(mh), 1, sizeof(buf));
    UT_ASSERT_EQUAL(0, n);

    n = examsgSend(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, (Examsg*)buf, sizeof(buf));
    UT_ASSERT_EQUAL(-EMSGSIZE, n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_EQUAL(0, n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(send_inexistant_mbox)
{
    ExamsgHandle mh;
    int n;
    ExamsgAny msg;

    /* init */
    exalog_as(EXAMSG_TEST_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);

    n = examsgSend(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST, &msg, sizeof(msg));
    UT_ASSERT_EQUAL(-ENXIO, n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_EQUAL(-EINVAL, n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(invalid_params)
{
    int n;
    n = examsgExit(NULL);
    UT_ASSERT_EQUAL(-EINVAL, n);

    n = examsgDelMbox(NULL, 0);
    UT_ASSERT_EQUAL(-EINVAL, n);
}

ut_test(examsgSendWithAck)
{
    ExamsgHandle mh;
    int n, ackError;
    Examsg msg;

    /* init */
    exalog_as(EXAMSG_TEST_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_TEST_ID);
    UT_ASSERT(mh != NULL);
    n = examsgAddMbox(mh, examsgOwner(mh), 2/* enougth for a examsg and a ack msg */, sizeof(msg));
    UT_ASSERT_VERBOSE(n == 0, "examsgAddMbox failed: got %d, expected 0", n);

    /* Real stuff */
    msg.any.type = 1234;

    /* Send the ack before asking for it is a trick to prevent from creating
     * a thread. */
    n = examsgAckReply(mh, &msg, 0x7DEADBEE/*error*/, EXAMSG_TEST_ID,
	               EXAMSG_LOCALHOST);
    UT_ASSERT_VERBOSE(n == 0, "examsgAckReply failed: got %d, expected 0", n);

    n = examsgSendWithAck(mh, EXAMSG_TEST_ID, EXAMSG_LOCALHOST,
	                  &msg, sizeof(msg), &ackError);
    UT_ASSERT_VERBOSE(n == sizeof(msg), "examsgSendWithAck failed: got %d, expected 0", n);
    UT_ASSERT_EQUAL(0x7DEADBEE, ackError);

    /* Send with ask with invalid mbox */
    n = examsgSendWithAck(mh, EXAMSG_TEST_ID + 1, EXAMSG_LOCALHOST,
	                  &msg, sizeof(msg), &ackError);
    UT_ASSERT_EQUAL(-ENXIO, n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_VERBOSE(n == 0, "examsgDelMbox failed: got %d, expected 0", n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

ut_test(examsgSend_to_network)
{
    ExamsgHandle mh;
    int n;
    exa_nodeset_t dest_nodes;
    Examsg msg;

    /* init */
    exalog_as(EXAMSG_NETMBOX_ID);
    n = examsg_static_init(EXAMSG_STATIC_CREATE);
    UT_ASSERT(n == 0);
    mh = examsgInit(EXAMSG_NETMBOX_ID);
    UT_ASSERT(mh != NULL);
    n = examsgAddMbox(mh, examsgOwner(mh), 2/* enougth for a examsg and a ack msg */, sizeof(msg));
    UT_ASSERT_VERBOSE(n == 0, "examsgAddMbox failed: got %d, expected 0", n);

    /* Send hosts */
    memset(&dest_nodes, 0xA /* 1010 binary */, sizeof(dest_nodes));
    n = examsgSend(mh, EXAMSG_NETMBOX_ID, &dest_nodes, &msg, sizeof(msg));
    UT_ASSERT_VERBOSE(n == sizeof(msg), "examsgSendWithAck failed: got %d, expected 0", n);

    /* cleanup */
    n = examsgDelMbox(mh, examsgOwner(mh));
    UT_ASSERT_VERBOSE(n == 0, "examsgDelMbox failed: got %d, expected 0", n);
    n = examsgExit(mh);
    UT_ASSERT_VERBOSE(n == 0, "examsgExit failed: got %d, expected 0", n);
    examsg_static_clean(EXAMSG_STATIC_DELETE);
}

