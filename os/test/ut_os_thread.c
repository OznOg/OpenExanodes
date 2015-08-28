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
#include "os/include/os_thread.h"
#include "os/include/os_time.h"
#include "os/include/os_atomic.h"

UT_SECTION(os_thread_create)

#define NB_THREAD 100
typedef struct
{
    volatile int started;
} thread_param_t;

thread_param_t thread_param[NB_THREAD];


static void thread_test_0(void * arg)
{
    thread_param_t * param = (thread_param_t *) arg;
    param->started = 1;
}


ut_test(os_thread_create)
{
    os_thread_t thread;
    thread_param[0].started = 0;
    UT_ASSERT(os_thread_create(&thread, 0, thread_test_0, &thread_param[0]));
    os_sleep(1); /* 1 second to start a thread seems correct, but infinite is better */
    UT_ASSERT(thread_param[0].started == 1); /* validate thread started and so validate arg correctly used*/
}

ut_test(create_join_bunch_of_thread)
{
     int i;
     os_thread_t thread[NB_THREAD];
     for (i = 0; i < NB_THREAD; i++)
         thread_param[i].started = 0;

     for (i = 0; i < NB_THREAD; i++)
     {
         UT_ASSERT(os_thread_create(&thread[i], 0, thread_test_0, &thread_param[i]));
     }
     for (i = 0; i < NB_THREAD; i++)
     {
         os_thread_join(thread[i]);
         UT_ASSERT(thread_param[i].started == 1);
     }
}


struct
{
    os_thread_mutex_t mutex;
    bool flip_flop;
    volatile bool fin;
} mutex_test;


#define MUTEX_MIN_TEST 1000

static void thread_test_mutex(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (!mutex_test.fin)
    {
        os_thread_mutex_lock(&mutex_test.mutex);

        if (!mutex_test.flip_flop)
            mutex_test.flip_flop = true;
	os_thread_mutex_unlock(&mutex_test.mutex);

        os_millisleep(20);
    }
}

/**
 *  * Two thread try to acces to same mutex
 */
ut_test(mutex)
{
    int count = 0;
    os_thread_t thread;
    os_thread_mutex_init(&mutex_test.mutex);
    mutex_test.fin = false;

    os_thread_create(&thread, 0, thread_test_mutex, NULL);

    while (!mutex_test.fin)
    {
        os_thread_mutex_lock(&mutex_test.mutex);

        /* sleep while holding the lock to force child to block on the mutex */
        if (count % 2 == 0)
            os_millisleep(30);
        if (mutex_test.flip_flop)
            mutex_test.flip_flop = false;

        os_thread_mutex_unlock(&mutex_test.mutex);

        if (++count >= MUTEX_MIN_TEST)
            mutex_test.fin = true;
    }

   os_thread_join(thread);
   os_thread_mutex_destroy(&mutex_test.mutex);
}


#ifndef WIN32 /* The two following unit tests do not pass on Windows in Virtualbox. */

#define LOOP_THREAD 2
#define ATOMIC_TAB_SIZE 32768
#define ATOMIC_TAB_DECAL 3

struct
{
    char tab[ATOMIC_TAB_SIZE];
    volatile int * tab_int;
    os_atomic_t * tab_atomic_int;
    volatile int begin;
    volatile int value;
    volatile int bug_found;
    volatile int fin;
    os_atomic_t atomic_value;
} atomic_test;

static void thread_and_atomic_launch(int nb_thread, void (*routine)(void *))
{
   os_thread_t thread[nb_thread];
   int i;
   void * arg;
   atomic_test.begin = 0;
   atomic_test.fin = 0;
   for (i = 0; i < nb_thread; i++)
   {
#ifdef WIN32
       arg = (void *)(INT_PTR)i;
#else
       arg = (void *)(long)i;
#endif
       UT_ASSERT(os_thread_create(&thread[i], 0, routine, arg));
   }
   os_sleep(1); /* all thread is now in the first busy loop*/
   atomic_test.begin = 1;
   os_sleep(5);
   atomic_test.fin = 1;
   for (i = 0; i < LOOP_THREAD; i++)
   {
       os_thread_join(thread[i]);
   }
}


static void thread_test_noatomic_inc_dec(void * arg)
{
   UNUSED_PARAMETER(arg);

   while (atomic_test.begin == 0)
	   ; /* yes it's a busy loop */
   while (atomic_test.fin == 0)
   {
       int j;
       for (j = 0; j < 1024; j ++)
       {
           atomic_test.value ++;
       }
       for (j = 0; j < 1024; j ++)
       {
          atomic_test.value --;
       }
   }
}


static void thread_test_atomic_inc_dec(void * arg)
{
    UNUSED_PARAMETER(arg);

    while (atomic_test.begin == 0)
       ; /* yes it's a busy loop */
    while (atomic_test.fin == 0)
    {
	int j;
        for (j = 0; j < 1024; j ++)
        {
            os_atomic_inc(&atomic_test.atomic_value);
        }
        for (j = 0; j < 1024; j ++)
        {
            os_atomic_dec(&atomic_test.atomic_value);
        }
   }
}


ut_test(thread_and_atomic_inc_dec)
{
    atomic_test.value = 0;
    thread_and_atomic_launch(LOOP_THREAD, thread_test_noatomic_inc_dec);

    /* test ++/-- multi thread without atomic */
    if (atomic_test.value == 0)
    {
        ut_printf("You doesn't have multicore or you schedule quantum is to high so atomic, mutex and semaphore unit test cannot be trusted\n");
    }

    UT_ASSERT(atomic_test.value != 0);

    /* test ++/-- multi thread without atomic */
    os_atomic_set(&atomic_test.atomic_value, 0);
    thread_and_atomic_launch(LOOP_THREAD, thread_test_atomic_inc_dec);
    UT_ASSERT(os_atomic_read(&atomic_test.atomic_value) == 0);
}


#define RW_MIN_CONFLICT 1024

struct
{
  os_thread_rwlock_t rwlock;
  os_atomic_t nb_reader;
  int multi_reader;
  volatile int fin;
} rwlock_test;


static void thread_test_rwlock(void * arg)
{
    int multi_reader;
    thread_param_t * param = (thread_param_t *) arg;
    param->started = 1;

    os_thread_rwlock_rdlock(&rwlock_test.rwlock);
    os_atomic_inc(&rwlock_test.nb_reader);
    os_sleep(1);
    /* after 1 seconds we expect all thread have get rdlock */
    UT_ASSERT(os_atomic_read(&rwlock_test.nb_reader) == NB_THREAD);
    /* NB_THREAD can get rdlock at same time is VALIDATED */
    os_sleep(1);
    os_atomic_dec(&rwlock_test.nb_reader);

    os_thread_rwlock_unlock(&rwlock_test.rwlock);
    do
    {
        os_thread_rwlock_rdlock(&rwlock_test.rwlock);
	multi_reader = os_atomic_read(&rwlock_test.nb_reader);
        os_atomic_inc(&rwlock_test.nb_reader);
        os_atomic_dec(&rwlock_test.nb_reader);
        os_thread_rwlock_unlock(&rwlock_test.rwlock);
	if (multi_reader > 0)
		/* more than one have readlock so update multi_reader with writelock */
	{
	    os_thread_rwlock_wrlock(&rwlock_test.rwlock);
	    /* check rwlock confict */
	    UT_ASSERT(os_atomic_read(&rwlock_test.nb_reader) == 0);
	    os_atomic_inc(&rwlock_test.nb_reader);
            rwlock_test.multi_reader++;
	    if (rwlock_test.multi_reader > RW_MIN_CONFLICT)
	    {
                rwlock_test.fin = 1;
	    }
            os_atomic_dec(&rwlock_test.nb_reader);
	    UT_ASSERT(os_atomic_read(&rwlock_test.nb_reader) == 0);
	    os_thread_rwlock_unlock(&rwlock_test.rwlock);
	}
    } while ( rwlock_test.fin == 0);
}


ut_test(thread_rwlock_and_atomic)
{
    int i;
    os_thread_t thread[NB_THREAD];

    os_thread_rwlock_init(&rwlock_test.rwlock);
    os_atomic_set(&rwlock_test.nb_reader, 0);
    rwlock_test.multi_reader = 0;
    rwlock_test.fin = 0;

    for (i = 0; i < NB_THREAD; i++)
        thread_param[i].started = 0;
    for (i = 0; i < NB_THREAD; i++)
    {
        UT_ASSERT(os_thread_create(&thread[i], 0, thread_test_rwlock, &thread_param[i]));
    }
    for (i = 0; i < NB_THREAD; i++)
    {
        os_thread_join(thread[i]);
        UT_ASSERT(thread_param[i].started == 1);
    }

    os_thread_rwlock_destroy(&rwlock_test.rwlock);
}

#endif /* !WIN32 */
