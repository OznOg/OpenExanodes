/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include <string.h>

#include "os/include/os_compiler.h"
#include "os/include/os_semaphore.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

UT_SECTION(os_semaphore)


struct
{
    os_sem_t sem;
    int thread_sem_has_lock;
    int thread_main_has_lock;
    int thread_sem_nb;
    int thread_main_nb;
    volatile int fin;
} sem_test;


#define SEM_MIN_TEST 1048576
#define SEM_SLEEP 262143

static void thread_test_sem(void * arg)
{
    UNUSED_PARAMETER(arg);

    do
    {
       UT_ASSERT_EQUAL(0, os_sem_wait(&sem_test.sem));
       UT_ASSERT(sem_test.thread_sem_has_lock == 0);
       UT_ASSERT(sem_test.thread_main_has_lock == 0);
       sem_test.thread_sem_has_lock = 1;
       sem_test.thread_sem_nb++;
       if ((sem_test.thread_sem_nb & SEM_SLEEP) == 0)
           os_sleep(1);
        UT_ASSERT(sem_test.thread_sem_has_lock == 1);
        UT_ASSERT(sem_test.thread_main_has_lock == 0);
        sem_test.thread_sem_has_lock = 0;
        os_sem_post(&sem_test.sem);
        if (sem_test.fin == 1)
            break;
    } while (1);
}

ut_test(semaphore_wait_timeout)
{
    int n;
    os_sem_init(&sem_test.sem, 0);
    n = os_sem_waittimeout(&sem_test.sem, 1999);
    UT_ASSERT_EQUAL(-ETIMEDOUT, n);

    os_sem_post(&sem_test.sem);
    n = os_sem_waittimeout(&sem_test.sem, 1999);
    UT_ASSERT_EQUAL(0, n);
}

/**
 * Two thread try to acces to same semaphore
 */
ut_test(semaphore_as_mutex) __ut_lengthy
{
    os_thread_t thread;
    os_sem_init(&sem_test.sem, 1);
    sem_test.thread_sem_has_lock = 0;
    sem_test.thread_main_has_lock = 0;
    sem_test.thread_sem_nb = 0;
    sem_test.thread_main_nb = 0;
    sem_test.fin = 0;
    os_thread_create(&thread, 0, thread_test_sem, NULL);
    os_sleep(1);
    do
    {
        UT_ASSERT_EQUAL(0, os_sem_wait(&sem_test.sem));

        UT_ASSERT(sem_test.thread_sem_has_lock == 0);
        UT_ASSERT(sem_test.thread_main_has_lock == 0);
        sem_test.thread_main_has_lock = 1;
        sem_test.thread_main_nb++;
        if ((sem_test.thread_main_nb & SEM_SLEEP) == 0)
            os_sleep(1);
        UT_ASSERT(sem_test.thread_sem_has_lock == 0);
        UT_ASSERT(sem_test.thread_main_has_lock == 1);
        sem_test.thread_main_has_lock = 0;
        if ((sem_test.thread_main_nb > SEM_MIN_TEST)
            && (sem_test.thread_sem_nb > SEM_MIN_TEST))
            sem_test.fin = 1;
        os_sem_post(&sem_test.sem);
   } while (sem_test.fin == 0);
   os_thread_join(thread);
}


ut_test(os_sem_init)
{
    os_sem_t sem;
    int ret;

    ret = os_sem_init(&sem, 1);
    UT_ASSERT_EQUAL(0, ret);

    if (ret == 0)
        os_sem_destroy(&sem);
}
