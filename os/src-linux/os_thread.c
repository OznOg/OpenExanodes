/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_compiler.h"
#include "os/include/os_mem.h"
#include "os/include/os_thread.h"
#include <stdlib.h>

typedef struct {
    void (*routine)(void*);
    void *arg;
} launcher_data_t;

static __thread launcher_data_t *launcher_data = NULL;

void os_thread_mutex_init(os_thread_mutex_t *mutex)
{
    pthread_mutex_init(mutex, NULL);
}

void os_thread_mutex_lock(os_thread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

void os_thread_mutex_unlock(os_thread_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

/**
 * @returns true if the mutex is granted, false otherwise.
 *          WARNING! This differs from the return value of
 *          pthread_mutex_trylock() that returns 0 in case
 *          of success.
 */
bool os_thread_mutex_trylock(os_thread_mutex_t *mutex)
{
    return pthread_mutex_trylock(mutex) == 0;
}

void os_thread_mutex_destroy(os_thread_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

void os_thread_rwlock_init(os_thread_rwlock_t *lock)
{
    pthread_rwlock_init(lock, NULL);
}

void os_thread_rwlock_rdlock(os_thread_rwlock_t *lock)
{
   pthread_rwlock_rdlock(lock);
}

void os_thread_rwlock_wrlock(os_thread_rwlock_t *lock)
{
    pthread_rwlock_wrlock(lock);
}

void os_thread_rwlock_unlock(os_thread_rwlock_t *lock)
{
    pthread_rwlock_unlock(lock);
}

void os_thread_rwlock_destroy(os_thread_rwlock_t *lock)
{
    pthread_rwlock_destroy(lock);
}

/* The windows API needs a pointer to be returned, but in our code, thread
 * creation returns void, thus we use this function to wrap the void function
 * and ALWAYS return NULL.
 */
static void *launcher(void *param)
{
    OS_ASSERT(launcher_data == NULL);
    launcher_data = (launcher_data_t *)param;

    launcher_data->routine(launcher_data->arg);

    OS_ASSERT(launcher_data != NULL);
    os_free(launcher_data);
    launcher_data = NULL;

    return NULL;
}

bool os_thread_create(os_thread_t *thread, int stacksize,
	              void (*start_routine)(void*), void *arg)
{
    pthread_attr_t attr;
    /* The malloc is authorized creating a thread is already a memory
     * consuming operation.
     * BTW this MUST be malloced as data are passed from a thread context to
     * another. */
    launcher_data_t *ld = (launcher_data_t *)os_malloc(sizeof(launcher_data_t));

    ld->arg = arg;
    ld->routine = start_routine;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stacksize);

    return pthread_create(thread, &attr, launcher, ld) == 0;
}

void os_thread_join(os_thread_t handle)
{
    pthread_join(handle, NULL);
}

void os_thread_detach(os_thread_t handle)
{
    pthread_detach(handle);
}

