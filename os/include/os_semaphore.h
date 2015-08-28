/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _LINUX_SEM_H
#define _LINUX_SEM_H

#ifdef WIN32
#include "os/include/os_windows.h"
typedef HANDLE os_sem_t;
#else
#include <semaphore.h>
typedef sem_t os_sem_t;
#endif


/**
 * Initialize a private semaphore.
 *
 * The semaphore is only accessible from within the process
 * that created it.
 *
 * @param sem    The semaphore to initialize
 * @param value  Value to init semaphore with
 *
 * @return 0 if successfull -1 otherwise
 *
 * @os_replace{Linux, sem_init}
 * @os_replace{Windows, CreateSemaphore}
 */
int os_sem_init(os_sem_t *sem, int value);

/**
 * Interruptible wait on a semaphore.
 *
 * When the function returns, the semaphore may not have been acquired due to
 * an interruption. This function asserts in case of error (an interruption
 * is not an error).
 *
 * NOTE: you probably want to use the non-interruptible version os_sem_wait()
 * instead.
 *
 * @param sem  Semaphore to wait on
 *
 * @return 0 if successful, or -EINTR in case of interruption
 *
 * @os_replace{Linux, sem_wait}
 * @os_replace{Windows, WaitForSingleObject}
 */
int os_sem_wait_intr(os_sem_t *sem);

/**
 * Non-interruptible wait on a semaphore.
 *
 * This function asserts in case of error.
 *
 * @param sem  Semaphore to wait on
 *
 * @return always 0
 *
 * @os_replace{Linux, sem_wait}
 * @os_replace{Windows, WaitForSingleObject}
 */
int os_sem_wait(os_sem_t *sem);

/**
 * Signal the private semaphore.
 *
 * @param sem  The semaphore
 *
 * @return 0 if successfull -1 otherwise
 *
 * @os_replace{Linux, sem_post}
 * @os_replace{Windows, ReleaseSemaphore}
 */
void os_sem_post(os_sem_t *sem);

/**
 * Wait on a private semaphore with a specified timeout
 * This function is interruptible by system calls.
 *
 * @param sem         The semaphore
 * @param timeout_ms  Timeout in milliseconds
 *
 * @return 0 if successfull or a negative error code
 *           In case of timeout, return value is -ETIMEDOUT
 *
 * @os_replace{Linux, sem_timedwait}
 * @os_replace{Windows, WaitForSingleObjectEx}
 */
int os_sem_waittimeout(os_sem_t *sem, int timeout_ms);

/**
 *
 *
 * @param sem  The semaphore
 *
 * @return 0 if successfull -1 otherwise
 *
 * @os_replace{Linux, sem_destroy}
 * @os_replace{Windows, CloseHandle}
 */
void os_sem_destroy(os_sem_t *sem);

#endif  // _LINUX_SEM_H
