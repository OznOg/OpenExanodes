/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __OS_IPC_SEM_H
#define __OS_IPC_SEM_H

#include "os/include/os_inttypes.h"
#include "os/include/os_time.h"

struct os_ipc_semset;
typedef struct os_ipc_semset os_ipc_semset_t;

/**
 * Create an set of nb_sems ipc semaphores.
 *
 * @os_replace{Linux, semctl}
 * @os_replace{Windows, CreateSemaphore}
 */
os_ipc_semset_t *__os_ipc_semset_create(uint32_t key, int nb_sems,
                                        const char *file, unsigned int line);
#define os_ipc_semset_create(key, nb_sems) \
    __os_ipc_semset_create(key, nb_sems, __FILE__, __LINE__);

/**
 * Get a os_ipc_semset_t on an existing semset
 *
 * @os_replace{Linux, semget}
 * @os_replace{Windows, OpenSemaphore}
 */
os_ipc_semset_t *__os_ipc_semset_get(uint32_t key, int nb_sems, bool create,
                                     const char *file, unsigned int line);
#define os_ipc_semset_get(key, nb_sems) \
    __os_ipc_semset_get(key, nb_sems, false, __FILE__, __LINE__);

/**
 * Delete a set of semaphore created with os_ipc_semset_create
 *
 * @os_replace{Linux, semctl}
 * @os_replace{Windows, CloseHandle}
 */
void __os_ipc_semset_delete(os_ipc_semset_t *semset, const char *file,
                            unsigned int line);
#define os_ipc_semset_delete(semset) \
    __os_ipc_semset_delete(semset, __FILE__, __LINE__);

/**
 * Release a set of semaphore retrieved with os_ipc_semset_get
 *
 * @os_replace{Linux, semctl}
 * @os_replace{Windows, CloseHandle}
 */
void __os_ipc_semset_release(os_ipc_semset_t *semset, const char *file,
                             unsigned int line);
#define os_ipc_semset_release(semset) \
    __os_ipc_semset_release(semset, __FILE__, __LINE__);

/**
 *
 * @os_replace{Linux, semtimedop}
 * @os_replace{Windows, WaitForSingleObject}
 */
void os_ipc_sem_down(os_ipc_semset_t *semset, int id);

/**
 *
 * @os_replace{Linux, semop}
 * @os_replace{Windows, ReleaseSemaphore}
 */
void os_ipc_sem_up(os_ipc_semset_t *semset, int id);

/**
 * Do a down on a semaphore with the guaranty that the caller wont be blocked
 * more than timeout.
 * When the timeout is reached this function returns -ETIME.
 * The value timeout is updated (the time spent in the call is substacted from
 * the value given in input).
 * When called with timeout = 0 with a pending up, the function returns 0 (the
 * up operation is prioritary on the timeout)
 * When called with timeout = NULL, the caller remains blocked until a up
 * incomes OR an interruption occurs; in the later case, -EINTR is returned.
 *
 * \param semset  a set of semaphore allocated with \a os_ipc_semset_create
 * \param id      the semaphore id in the semset
 * \param tiemout Max duration of the call
 *
 * \return 0 when the down was successful, -ETIME when the timeout is reached
 *           or -EINTR if a signal interrupted the system call.
 *
 * @os_replace{Linux, semtimedop}
 * @os_replace{Windows, WaitForSingleObject}
 */
int os_ipc_sem_down_timeout(os_ipc_semset_t *semset, int id, struct timeval *_timeout);

#ifdef USE_SEMGETVAL
/*
 * This function is for unit test purpose, and should not be used outside
 * of unit tests (or for debugging purpose). The Windows implementation
 * can be considered as a hugly hack as there is no clean API for doing this
 * on Windows (this is part of the 'hidden' API).
 * DO NOT USE ELSEWHERE
 */
int os_ipc_sem_getval(os_ipc_semset_t *semset, int id);
#endif

#endif
