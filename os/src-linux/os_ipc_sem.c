/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <sys/stat.h>  /* for mode_t */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "os/include/os_assert.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_ipc_sem.h"
#include "os/include/os_mem.h"

/* see semctl man for this structure */
union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
				(Linux specific) */
};

#define IGNORED 0

struct os_ipc_semset {
    uint32_t key;
    int      nb_sems;
    int      semid; /* the one returned by semget */
};

os_ipc_semset_t *__os_ipc_semset_get(uint32_t key, int nb_sems,
                                     bool create, const char *file,
                                     unsigned int line)
{
    int flags;
    os_ipc_semset_t *semset;

    OS_ASSERT(nb_sems >= 0);

    /* Allocate our metadata */
    semset = os_malloc_trace(sizeof(os_ipc_semset_t), file, line);
    if (!semset)
	return NULL;

    flags = S_IRUSR | S_IWUSR;
    if (create)
	flags = flags | IPC_CREAT;
    /* FIXME IPC_EXCL flags should be used, but it is difficult since our
     * daemons do not stop properly ...*/

    /* Try to create semaphore array */
    semset->semid = semget(key, nb_sems, flags);
    if (semset->semid == -1)
    {
	os_free_trace(semset, file, line);
	return NULL;
    }

    semset->nb_sems = nb_sems;
    semset->key = key;

    return semset;
}

void __os_ipc_semset_release(os_ipc_semset_t *semset, const char *file,
                             unsigned int line)
{
    os_free_trace(semset, file, line);
}

/* create an set of nb_sems ipc semaphores. */
os_ipc_semset_t *__os_ipc_semset_create(uint32_t key, int nb_sems,
                                        const char *file, unsigned int line)
{
    int ret;
    union semun argument;
    unsigned short array[nb_sems];
    os_ipc_semset_t *semset;

    OS_ASSERT(nb_sems >= 0);

    /* Create os semaphores */
    semset = __os_ipc_semset_get(key, nb_sems, true, file, line);
    if (!semset)
	return NULL;

    /* Build initialization array */
    memset(array, 0, sizeof(array));
    argument.array = array;

    /* Try to init all sems */
    ret = semctl(semset->semid, IGNORED, SETALL, argument);
    if (ret)
    {
	/* It failed :/ */
	semctl(semset->semid, IGNORED, IPC_RMID);
	os_free_trace(semset, file, line);
	return NULL;
    }

    return semset;
}

void __os_ipc_semset_delete(os_ipc_semset_t *semset, const char *file,
                            unsigned int line)
{
    semctl(semset->semid, IGNORED, IPC_RMID);

    __os_ipc_semset_release(semset, file, line);
}

int os_ipc_sem_down_timeout(os_ipc_semset_t *semset, int id, struct timeval *_timeout)
{
    int ret;
    struct timespec timeout, before, after;
    struct sembuf op;

    OS_ASSERT(id >= 0 && id < semset->nb_sems);

    op.sem_num = id;
    op.sem_op = -1;        /* decrement sem */
    op.sem_flg = 0;

    /* build the timespec */
    if (_timeout)
    {
        OS_ASSERT(TIMEVAL_IS_VALID(_timeout));
	clock_gettime(CLOCK_MONOTONIC, &before);

	timeout.tv_sec = _timeout->tv_sec;
	timeout.tv_nsec = _timeout->tv_usec * 1000;

        OS_ASSERT(TIMESPEC_IS_VALID(&timeout));
    }

    ret = semtimedop(semset->semid, &op, 1, _timeout ? &timeout : NULL);

    if (ret == -1 && errno == EAGAIN)
	ret = -ETIME;

    if (ret == -1 && errno == EINTR)
	ret = -EINTR;

    OS_ASSERT_VERBOSE(ret != -1, "Cannot lock: error %d", errno);

    /* update input timeout */
    if (_timeout)
    {
	if (ret == -ETIME)
	{
	    _timeout->tv_sec = 0;
	    _timeout->tv_usec = 0;
	    return ret;
	}

	/* semtimedop does NOT update the timeout param (the time left) so
	 * we compute it here */
	clock_gettime(CLOCK_MONOTONIC, &after);

	_timeout->tv_sec -= after.tv_sec - before.tv_sec;
        _timeout->tv_usec -= (after.tv_nsec - before.tv_nsec) / 1000;
	if (_timeout->tv_usec < 0)
	{
	    _timeout->tv_sec--;
	    _timeout->tv_usec += 1000000;
	}
	else if (_timeout->tv_usec >= 1000000)
	{
            _timeout->tv_sec++;
	    _timeout->tv_usec -= 1000000;
	}
	if (_timeout->tv_sec < 0)
	{
	    _timeout->tv_sec = 0;
	    _timeout->tv_usec = 0;
	    /* here we do not return ETIME because the down was actually done;
	     * This is just a fact that the time left is 0 */
	}
	OS_ASSERT(_timeout->tv_usec >= 0);
	OS_ASSERT(_timeout->tv_usec < 1000000);
    }

    return ret;
}

void os_ipc_sem_down(os_ipc_semset_t *semset, int id)
{
    int ret;
    do
	ret = os_ipc_sem_down_timeout(semset, id, NULL);
    while (ret == -EINTR);
}

void os_ipc_sem_up(os_ipc_semset_t *semset, int id)
{
    int ret;
    struct sembuf op;
    OS_ASSERT(id >= 0 && id < semset->nb_sems);

    op.sem_num = id;
    op.sem_op = 1;         /* increment sem */
    op.sem_flg = 0;

    do
	ret = semop(semset->semid, &op, 1);
    while (ret == -1 && errno == EINTR);

    OS_ASSERT_VERBOSE(ret != -1, "Cannot unlock: error %d", errno);
}

int os_ipc_sem_getval(os_ipc_semset_t *semset, int id)
{
    int val;
    do
	val = semctl(semset->semid, id, GETVAL);
    while (val == -1 && errno == EINTR);

    OS_ASSERT_VERBOSE(val != -1, "Cannot get value: error %d", errno);
    return val;
}
