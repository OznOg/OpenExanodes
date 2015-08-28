/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_SYNC_JOB_H__
#define __RAIN1_SYNC_JOB_H__

#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_rdev.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_semaphore.h"

typedef enum
{
#define SYNC_JOB_STEP__FIRST SYNC_JOB_WRITE
    SYNC_JOB_WRITE,
    SYNC_JOB_UNLOCK,
    SYNC_JOB_IDLE
#define SYNC_JOB_STEP__LAST SYNC_JOB_IDLE
} sync_job_step_t;

#define SYNC_JOB_STEP_IS_VALID(step)  \
    ((step) >= SYNC_JOB_STEP__FIRST && (step) <= SYNC_JOB_STEP__LAST)

/**
 * Structure representing a job, used during resync and rebuild
 * operations. Each job has its own bio to perform read and write
 * I/Os, and the jobs are handled separatly and asynchronously by
 * rain1_su_synchronize.
 */
typedef struct
{
    blockdevice_io_t bio;
    void * buffer;
    uint64_t su;
    unsigned long part;
    sync_job_step_t step;
    struct vrt_realdev *locked_rd;
    uint64_t locked_rd_sector_start;
    bool completed;
    bool active;
    int error;
    os_sem_t *sem;

    struct rdev_location dst_rdev_loc[3];
    struct rdev_location src_rdev_loc;
    blockdevice_io_t dst_bio[3];
    blockdevice_io_t src_bio;
    unsigned int nb_dst;
    unsigned int nb_dst_written;

} sync_job_t;

typedef struct sync_job_pool
{
    unsigned int nb_jobs;
    unsigned int block_size;
    sync_job_t *jobs;
    os_sem_t sem;
} sync_job_pool_t;

/**
 * Pre-allocate sync jobs at dgstart in order to avoid to allocate memory in resync
 * and rebuild
 */
sync_job_pool_t *sync_job_pool_alloc(unsigned int blksize, unsigned int nb_jobs);

/**
 * Free sync jobs allocated in sync_jobs_alloc
 */
void __sync_job_pool_free(sync_job_pool_t *job_pool);
#define sync_job_pool_free(job_pool) (__sync_job_pool_free(job_pool), job_pool = NULL)

/**
 * Initialize all the jobs of a given pool
 */
void sync_job_pool_init(sync_job_pool_t *job_pool);

/**
 * Unlock all remaining locked zones and clear the pool
 */
void sync_job_pool_clear(sync_job_pool_t *job_pool);

/**
 * Test if there is any active job in the pool
 */
bool sync_job_pool_is_any_active(const sync_job_pool_t *job_pool);

/**
 * Signals that a job has completed one step
 */
void sync_job_signal_step_completed(sync_job_t *job);

/**
 * Wait for a any job to complete a step
 */
void sync_job_pool_wait_step_completion(sync_job_pool_t *job_pool);

void sync_job_prepare(sync_job_t *job, rain1_group_t *rxg,
                      bool rebuild, const slot_t *slot,
                      uint64_t su, uint64_t part);

int sync_job_lock(sync_job_t *job, uint64_t blksize);
int sync_job_unlock(sync_job_t *job, uint64_t blksize);
void sync_job_read(sync_job_t *job, uint64_t blksize);
void sync_job_write(sync_job_t *job, uint64_t blksize);

#endif
