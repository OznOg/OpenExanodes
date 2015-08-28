/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_THREAD_H
#define OS_THREAD_H

#include "os/include/os_inttypes.h"
#include "common/include/exa_assert.h"

#ifdef WIN32

#include "os/include/os_windows.h"

typedef HANDLE os_thread_t;
typedef CRITICAL_SECTION os_thread_mutex_t;

typedef struct
{
    SRWLOCK lock;
    int rd;
} os_thread_rwlock_t;

#else  /* WIN32 */

#include <pthread.h>
typedef pthread_rwlock_t os_thread_rwlock_t;
typedef pthread_mutex_t os_thread_mutex_t;
typedef pthread_t os_thread_t;

#endif  /* WIN32 */

/**
 * Initialize a thread mutex
 *
 * @param mutex  Mutex to initialize
 *
 * @os_replace{Linux, pthread_mutex_init}
 * @os_replace{Windows, InitializeCriticalSection}
 */
void os_thread_mutex_init(os_thread_mutex_t *mutex);

/**
 * Lock on a thread mutex
 *
 * @param mutex  Mutex to lock
 *
 * @os_replace{Linux, pthread_mutex_lock}
 * @os_replace{Windows, EnterCriticalSection}
 */
void os_thread_mutex_lock(os_thread_mutex_t *mutex);

/**
 * Unlock a thread mutex
 *
 * @param mutex  Mutex to unlock
 *
 * @os_replace{Linux, pthread_mutex_unlock}
 * @os_replace{Windows, LeaveCriticalSection}
 */
void os_thread_mutex_unlock(os_thread_mutex_t *mutex);

/**
 * try to lock a thread mutex
 *
 * @param mutex  Mutex to lock
 *
 * @return  true if successfully locked the mutex, false otherwise
 *
 * @os_replace{Linux, pthread_mutex_trylock}
 */
bool os_thread_mutex_trylock(os_thread_mutex_t *mutex);

/**
 * Destroy a mutex
 *
 * @param mutex  The mutex to destroy
 *
 * @os_replace{Linux, pthread_mutex_destroy}
 * @os_replace{Windows, DeleteCriticalSection}
 */
void os_thread_mutex_destroy(os_thread_mutex_t *mutex);

/**
 * Initialise a read-write lock
 *
 * @param lock  The lock to initialise
 *
 * @os_replace{Linux, pthread_rwlock_init}
 * @os_replace{Windows, InitializeSRWLock}
 */
void os_thread_rwlock_init(os_thread_rwlock_t *lock);

/**
 * Acquire a read lock on a read-write lock
 *
 * @param lock  The lock to acquire
 *
 * @os_replace{Linux, pthread_rwlock_rdlock}
 * @os_replace{Windows, AcquireSRWLockShare}
 */
void os_thread_rwlock_rdlock(os_thread_rwlock_t *lock);

/**
 * Acquire a write lock on a read-write lock
 *
 * @param lock  The lock to acquire
 *
 * @os_replace{Linux, pthread_rwlock_wrlock}
 * @os_replace{Windows, AcquireSRWLockExclusive}
 */
void os_thread_rwlock_wrlock(os_thread_rwlock_t *lock);

/**
 * unlock a read-write lock
 *
 * @param lock  The read-write lock to unlock
 *
 * @os_replace{Linux, pthread_rwlock_unlock}
 * @os_replace{Windows, ReleaseSRWLockExclusive, ReleaseSRWLockShared}
 */
void os_thread_rwlock_unlock(os_thread_rwlock_t *lock);

/**
 * Destroy a read-write lock
 *
 * @param lock  The lock to destroy
 *
 */
void os_thread_rwlock_destroy(os_thread_rwlock_t *lock);

/**
 * Create a new thread
 *
 * @param[out] thread    The id of the created thread
 * @param stacksize      The thread stack size
 * @param start_routine  The routine to execute when creating thread
 * @param arg            The arguments to pass to start routine
 *
 * @return true if thread successfully created, false otherwise
 *
 * @os_replace{Linux, pthread_attr_init, pthread_attr_setstacksize, pthread_create}
 * @os_replace{Windows, CreateThread}
 */
bool os_thread_create(os_thread_t *thread, int stacksize,
		      void (*start_routine)(void*), void *arg);

/* Thread cancelling is forbidden, find a clean way to end threads */
#define os_thread_cancel(handle) COMPILE_TIME_ASSERT(false)
#define os_thread_testcancel(handle) COMPILE_TIME_ASSERT(false)


/**
 * Terminate the calling thread
 *
 * If you need this function, it very likely that you are writing crappy code.
 * A thread should have a proper main loop and a correct reason to stop.
 * Os_thread_exit make it __very__ difficult to know how a thread exits and
 * makes cleanup of data nearly impossible.
 * DO NOT USE THIS FUNCTION NOR OS NATIVE ONES.
 * This does not link, and will never since I (Sebastien) am here.
 *
 * @os_replace{Linux, pthread_exit}
 * @os_replace{Windows, ExitThread}
 */
void os_thread_exit(void);

/**
 * Suspend execution of the calling thread until a target thread terminates
 *
 * @param handle  Id of the target thread to wait for
 *
 * @os_replace{Linux, pthread_join}
 * @os_replace{Windows, WaitForSingleObject, CloseHandle}
 */
void os_thread_join(os_thread_t handle);

/**
 * Detach a thread
 *
 * @param handle  id of the thread
 *
 * @os_replace{Linux, pthread_detach}
 * @os_replace{Windows, CloseHandle}
 */
void os_thread_detach(os_thread_t handle);

#endif

