/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "examsg/include/examsg.h"
#include "examsg/src/mailbox.h"
#include "os/include/os_time.h"
#include "os/include/os_thread.h"
#include "os/include/os_error.h"

#define AnID ((ExamsgID)0)
#define AnotherID ((ExamsgID)1)

ut_setup()
{
}

ut_cleanup()
{
}

ut_test(create_mailboxes_framework)
{
   int err = examsgMboxCreateAll();
   ut_printf("%d", err);
   UT_ASSERT(err == 0);
   examsgMboxDeleteAll();
}

ut_test(test_stats)
{
    ExamsgID id;
    int err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	err = examsgMboxCreate(id, id, 1, 1000);
	UT_ASSERT(err == 0);
    }

    examsgMboxShowStats();

    examsgMboxDeleteAll();
}

ut_test(create_a_mbox_and_send)
{
    int ret;
    char hello[] = "HELLO";
    int err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    err = examsgMboxCreate(AnID, AnID, 1, sizeof(hello));
    UT_ASSERT(err == 0);

    ret = __examsgMboxSend(AnID, AnID, EXAMSGF_NONE, hello, sizeof(hello), NULL);
    UT_ASSERT(ret == sizeof(hello));

    examsgMboxDeleteAll();
}

ut_test(create_a_mbox_and_send_receive)
{
    int ret;
    ExamsgMID mid, mid2;
    char hello[] = "It works !";
    int err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    memset(&mid, 0xED, sizeof(mid));

    err = examsgMboxCreate(AnID, AnID, 1, sizeof(hello) + sizeof(mid));
    UT_ASSERT(err == 0);

    ret = __examsgMboxSend(AnID, AnID, EXAMSGF_NONE, &mid, sizeof(mid),
	                   hello, sizeof(hello), NULL);
    UT_ASSERT(ret == sizeof(hello) + sizeof(mid));

    ret = examsgMboxRecv(AnID, &mid2, sizeof(mid2), hello, sizeof(hello));
    UT_ASSERT(ret == sizeof(hello) + sizeof(mid2));

    UT_ASSERT(!memcmp(&mid, &mid2, sizeof(mid)));

    ut_printf("%s", hello);

    examsgMboxDeleteAll();
}

ut_test(create_a_mbox_no_send_wait_timeout)
{
    int ret;
    mbox_set_t *mbox_set;
    struct timeval timeout;
    ExamsgMID mid, mid2;
    char hello[] = "HELLO";
    int err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    err = examsgMboxCreate(AnID, AnID, 1, sizeof(hello) + sizeof(mid));
    UT_ASSERT(err == 0);

    timeout.tv_sec = 1; /* 1 second */
    timeout.tv_usec = 0;

    mbox_set = mboxset_alloc(__FILE__, __LINE__);
    mboxset_add(mbox_set, AnID);

    do {
	ret = examsgMboxWait(AnID, mbox_set, &timeout);
    } while (ret == -EINTR);

    UT_ASSERT(ret == -ETIME);

    ret = examsgMboxRecv(AnID, &mid2, sizeof(mid2), hello, sizeof(hello));
    UT_ASSERT(ret == 0); /* Noting to receive */

    mboxset_free(mbox_set);
    examsgMboxDeleteAll();
}

ut_test(create_a_mbox_and_send_wait_and_receive)
{
    struct timeval timeout;
    mbox_set_t *mbox_set;
    int ret;
    ExamsgMID mid, mid2;
    char hello[] = "HELLO";
    int err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    err = examsgMboxCreate(AnID, AnID, 1, sizeof(hello) + sizeof(mid));
    UT_ASSERT(err == 0);

    ret = __examsgMboxSend(AnID, AnID, EXAMSGF_NONE, &mid, sizeof(mid),
	                   hello, sizeof(hello), NULL);
    UT_ASSERT(ret == sizeof(hello) + sizeof(mid));

    mbox_set = mboxset_alloc(__FILE__, __LINE__);
    mboxset_add(mbox_set, AnID);

    do {
	ret = examsgMboxWait(AnID, mbox_set, NULL);
    } while (ret == -EINTR);
    UT_ASSERT(ret == 0);

    ret = examsgMboxRecv(AnID, &mid2, sizeof(mid2), hello, sizeof(hello));
    UT_ASSERT(ret == sizeof(hello) + sizeof(mid2));

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;

    do {
	ret = examsgMboxWait(AnID, mbox_set, &timeout);
    } while (ret == -EINTR);
    UT_ASSERT(ret == -ETIME);

    mboxset_free(mbox_set);
    examsgMboxDeleteAll();
}

#define HELLO "HELLO"
static void child1(void *dummy)
{
    int res;
    ExamsgMID mid;
    char hello[] = HELLO;
    os_sleep(2);
    res = __examsgMboxSend((ExamsgID)3, AnotherID, EXAMSGF_NONE, &mid, sizeof(mid),
	    hello, sizeof(hello), NULL);
    UT_ASSERT(res == sizeof(hello) + sizeof(mid));
}

ut_test(Father_and_child)
{
    os_thread_t T;
    mbox_set_t *mbox_set;
    ExamsgMID fmid;
    char buffer[128];
    int ret, err;
    err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    err = examsgMboxCreate(AnID, AnotherID, 1, sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(err == 0);

    mbox_set = mboxset_alloc(__FILE__, __LINE__);
    mboxset_add(mbox_set, AnotherID);

    os_thread_create(&T, 0, child1, NULL);

    do {
	ret = examsgMboxWait(AnID, mbox_set, NULL);
    } while (ret == -EINTR);
    UT_ASSERT(ret == 0);

    ret = examsgMboxRecv(AnotherID, &fmid, sizeof(fmid), buffer, sizeof(buffer));
    UT_ASSERT(ret == sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(!strcmp(buffer, HELLO));

    os_thread_join(T);

    mboxset_free(mbox_set);
    examsgMboxDeleteAll();
}
#undef HELLO


#define CHILD_ID ((ExamsgID) 1)
#define FATHER_ID ((ExamsgID) 2)
#define HELLO "HELLO"
static void child2(void *dummy)
{
    int ret;
    char buffer[128];
    char hello[] = HELLO;
    mbox_set_t *cmbox_set;
    ExamsgMID mid;

    /* Check the father mbox is full */
    ret = __examsgMboxSend(CHILD_ID, FATHER_ID, EXAMSGF_NOBLOCK,
	    &mid, sizeof(mid),
	    hello, sizeof(hello), NULL);
    UT_ASSERT(ret == -ENOSPC);

    /* Try to send in blocking mode... and remain block until father read */
    ret = __examsgMboxSend(CHILD_ID, FATHER_ID, EXAMSGF_NONE,
	    &mid, sizeof(mid),
	    hello, sizeof(hello), NULL);
    /* Must eventually be successful */
    UT_ASSERT(ret == sizeof(HELLO) + sizeof(ExamsgMID));

    cmbox_set = mboxset_alloc(__FILE__, __LINE__);
    mboxset_add(cmbox_set, CHILD_ID);

    /* Wait for a new message (there MUST be one !) */
    do {
	ret = examsgMboxWait(CHILD_ID, cmbox_set, NULL);
    } while (ret == -EINTR);
    UT_ASSERT(ret == 0);

    /* Receive the message */
    ret = examsgMboxRecv(CHILD_ID, &mid, sizeof(mid),
	    buffer, sizeof(buffer));
    UT_ASSERT(ret == sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(!strcmp(buffer, HELLO));

    mboxset_free(cmbox_set);
}

ut_test(wait_on_mbox_full_and_simultaneous_receive)
{
    mbox_set_t *mbox_set;
    os_thread_t T;
    ExamsgMID fmid;
    char buffer[128];
    int ret, err;
    char hello[] = HELLO;

    /****** INIT *******/
    err = examsgMboxCreateAll();
    UT_ASSERT(err == 0);

    err = examsgMboxCreate(FATHER_ID, FATHER_ID, 1,
	                   sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(err == 0);

    err = examsgMboxCreate(CHILD_ID, CHILD_ID, 1,
	                   sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(err == 0);
    /* end init */

    /* Fill up the father mbox */
    ret = __examsgMboxSend(FATHER_ID, FATHER_ID, EXAMSGF_NONE,
	                    &fmid, sizeof(fmid), hello, sizeof(hello), NULL);
    UT_ASSERT(ret == sizeof(hello) + sizeof(fmid));

    os_thread_create(&T, 0, child2, NULL);

    os_sleep(1);

    /* Send a message to child */
    ret = __examsgMboxSend(FATHER_ID, CHILD_ID, EXAMSGF_NONE,
	                   &fmid, sizeof(fmid), hello, sizeof(hello), NULL);
    UT_ASSERT(ret == sizeof(hello) + sizeof(fmid));

    os_sleep(1);

    mbox_set = mboxset_alloc(__FILE__, __LINE__);
    mboxset_add(mbox_set, FATHER_ID);

    /* Wait and read first message */
    do {
	ret = examsgMboxWait(FATHER_ID, mbox_set, NULL);
    } while (ret == -EINTR);
    UT_ASSERT(ret == 0);

    ret = examsgMboxRecv(FATHER_ID, &fmid, sizeof(fmid), buffer, sizeof(buffer));
    UT_ASSERT(ret == sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(!strcmp(buffer, HELLO));

    /* Wait and read second message */
    do {
	ret = examsgMboxWait(FATHER_ID, mbox_set, NULL);
    } while (ret == -EINTR);
    UT_ASSERT(ret == 0);

    ret = examsgMboxRecv(FATHER_ID, &fmid, sizeof(fmid), buffer, sizeof(buffer));
    UT_ASSERT(ret == sizeof(HELLO) + sizeof(ExamsgMID));
    UT_ASSERT(!strcmp(buffer, HELLO));

    os_thread_join(T);

    mboxset_free(mbox_set);
    examsgMboxDeleteAll();
}

