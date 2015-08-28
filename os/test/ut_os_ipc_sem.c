/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "os/include/os_compiler.h"
#include "os/include/os_error.h"
#define USE_SEMGETVAL
#include "os/include/os_ipc_sem.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"
#include "os/include/os_stdio.h"

#define DUMMY_KEY 0x5EA45678
#define NB_SEMS 42

static os_ipc_semset_t *semset;

ut_setup()
{
#ifndef WIN32
    /* FIXME this way to cleanup is not really nice...
     * should probably implement a os_ipc_sem_cleanup(KEY) */
    char cleanup_cmd[256];

    os_snprintf(cleanup_cmd, sizeof(cleanup_cmd), "ipcrm -S 0x%X > /dev/null 2>&1", DUMMY_KEY);

    system(cleanup_cmd);
#endif

    semset = os_ipc_semset_create(DUMMY_KEY, NB_SEMS);
    UT_ASSERT(semset);
}

ut_cleanup()
{
    os_ipc_semset_delete(semset);
}

ut_test(simple_create_and_delete_semset)
{
    /* empty, all is doen in setup/clean */
}

#define NB_LOOP   19

/* Father is passed p != NULL and child p == NULL
 * so that the same loop can be used for both. */
static void test_funct(void *p)
{
#define FATHER_ID 12
#define CHILD_ID  24
    int i;
    int my_id = p ? FATHER_ID : CHILD_ID;
    int peer_id = p ? CHILD_ID : FATHER_ID;

    if (my_id == FATHER_ID)
	os_ipc_sem_up(semset, my_id);

    for (i = 0; i < NB_LOOP; i++)
    {
	os_ipc_sem_down(semset, my_id);
	os_ipc_sem_up(semset, peer_id);
    }
}

ut_test(Father_child_interleaved_test)
{
    os_thread_t T;

    os_thread_create(&T, 0, test_funct, NULL);

    test_funct((void *)(long)!NULL);

    os_thread_join(T);
}

ut_test(many_up_and_many_down)
{
    int i;

    /* This should NOT deadlock */
    for (i = 0; i < NB_LOOP; i++)
	os_ipc_sem_up(semset, CHILD_ID);

    for (i = 0; i < NB_LOOP; i++)
	os_ipc_sem_down(semset, CHILD_ID);
}

ut_test(up_all_sem_and_down_in_reverse_order)
{
    int id;

    /* This should NOT deadlock */
    for (id = 0; id < NB_SEMS; id++)
	os_ipc_sem_up(semset, id);

    for (id = 0; id < NB_SEMS; id++)
    {
	int val = os_ipc_sem_getval(semset, id);
	UT_ASSERT(val == 1);
    }

    for (--id; id >= 0; id--)
	os_ipc_sem_down(semset, id);

    for (id = 0; id < NB_SEMS; id++)
    {
	int val = os_ipc_sem_getval(semset, id);
	UT_ASSERT(val == 0);
    }


}

ut_test(timeout_facillity)
{
    struct timeval timeout;
    int ret;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; /* 100ms */

    ret = os_ipc_sem_down_timeout(semset, 0, &timeout);
    UT_ASSERT(ret == -ETIME);
    UT_ASSERT(timeout.tv_sec == 0);
    UT_ASSERT(timeout.tv_usec == 0);
}

ut_test(zero_lenth_timeout)
{
    struct timeval timeout;
    int ret;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    ret = os_ipc_sem_down_timeout(semset, 0, &timeout);
    UT_ASSERT(ret == -ETIME);
    UT_ASSERT(timeout.tv_sec == 0);
    UT_ASSERT(timeout.tv_usec == 0);

    /* Same test but with a pending up */
    os_ipc_sem_up(semset, 0);
    ret = os_ipc_sem_down_timeout(semset, 0, &timeout);
    UT_ASSERT(ret == 0);
    UT_ASSERT(timeout.tv_sec == 0);
    UT_ASSERT(timeout.tv_usec == 0);
}

#define LONG_TIME 10000
#define SLEEP_TIME 3
const int semid = 20;
static void child(void *dummy)
{
    os_ipc_semset_t *csemset;
    struct timeval timeout;
    int ret;

    UNUSED_PARAMETER(dummy);

    timeout.tv_sec = LONG_TIME;
    timeout.tv_usec = 0;

    /* Get a handle on existing semset */
    csemset = os_ipc_semset_get(DUMMY_KEY, NB_SEMS);
    UT_ASSERT(csemset != NULL);

    ret = os_ipc_sem_down_timeout(csemset, semid, &timeout);

    UT_ASSERT(ret == 0);
    /* Check we did actually wait */
    UT_ASSERT(timeout.tv_sec < LONG_TIME);
    /* Check the wait duration is 'almost' good (I accepte 1 second error) */
    UT_ASSERT(timeout.tv_sec >= LONG_TIME - SLEEP_TIME - 1 /* 1s */);
    UT_ASSERT(timeout.tv_sec <= LONG_TIME - SLEEP_TIME + 1 /* 1s */);
}

ut_test(timeout_child) __ut_lengthy
{
    os_thread_t T;
    os_thread_create(&T, 0, child, NULL);
    os_sleep(SLEEP_TIME);

    os_ipc_sem_up(semset, semid);

    os_thread_join(T);
}


