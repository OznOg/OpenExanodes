/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/layout/rain1/src/lay_rain1_sync_job.h"

#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "os/include/os_mem.h"

#include "vrt/layout/rain1/src/lay_rain1_striping.h"
#include "vrt/virtualiseur/include/vrt_request.h"

/**
 * Pre-allocate sync jobs at dgstart in order to avoid to allocate memory in resync
 * and rebuild
 */
sync_job_pool_t *sync_job_pool_alloc(unsigned int blksize, unsigned int nb_jobs)
{
    sync_job_pool_t *job_pool;
    int i;

    job_pool = os_malloc(sizeof(sync_job_pool_t));
    if (job_pool == NULL)
	return NULL;

    job_pool->jobs = os_malloc(nb_jobs * sizeof(sync_job_t));
    if (job_pool->jobs == NULL)
    {
        os_free(job_pool);
	return NULL;
    }

    job_pool->nb_jobs = nb_jobs;
    job_pool->block_size = blksize;

    /* Allocate the buffers for the IOs */
    for (i = 0; i < nb_jobs; i++)
    {
	sync_job_t *job = &job_pool->jobs[i];

        job->buffer = os_malloc(SECTORS_TO_BYTES(blksize));
        if (job->buffer == NULL)
        {
            __sync_job_pool_free(job_pool);
            return NULL;
        }

	job->sem = &job_pool->sem;
    }

    return job_pool;
}

void __sync_job_pool_free(sync_job_pool_t *job_pool)
{
    int i;

    if (job_pool != NULL && job_pool->jobs != NULL)
    {
        for (i = 0; i < job_pool->nb_jobs; i++)
            os_free(job_pool->jobs[i].buffer);

        os_free(job_pool->jobs);
    }

    os_free(job_pool);
}

void sync_job_pool_init(sync_job_pool_t *job_pool)
{
    unsigned int i;

    EXA_ASSERT(job_pool != NULL);
    EXA_ASSERT(job_pool->jobs != NULL);

    os_sem_init(&job_pool->sem, 0);

    for (i = 0; i < job_pool->nb_jobs; i++)
    {
	sync_job_t *job = &job_pool->jobs[i];

	job->completed = true;
	job->active    = true;
	job->error     = EXA_SUCCESS;
	job->step      = SYNC_JOB_IDLE;
	job->su        = 0;
	job->part      = 0;
	job->sem       = &job_pool->sem;

	job->locked_rd              = NULL;
	job->locked_rd_sector_start = 0;
    }
}

void sync_job_pool_clear(sync_job_pool_t *job_pool)
{
    unsigned int i;

    for (i = 0; i < job_pool->nb_jobs; i++)
    {
	sync_job_t *job = &job_pool->jobs[i];

	/* Unlock remaining locks */
	if (job->locked_rd)
        {
            exalog_warning("Job %u still got a lock su=%"PRIu64" and part=%"PRIu64
                           " -> force unlock",
                           i, job->su, job->part);
	    vrt_rdev_unlock_sectors(job->locked_rd,
                                    job->locked_rd_sector_start,
                                    job_pool->block_size);
        }
    }

    os_sem_destroy(&job_pool->sem);
}

bool sync_job_pool_is_any_active(const sync_job_pool_t *job_pool)
{
    unsigned int i;

    EXA_ASSERT(job_pool != NULL);

    for (i = 0; i < job_pool->nb_jobs; i++)
	if (job_pool->jobs[i].active)
	    return true;

    return false;
}

void sync_job_signal_step_completed(sync_job_t *job)
{
    os_sem_post(job->sem);
}

void sync_job_pool_wait_step_completion(sync_job_pool_t *job_pool)
{
    os_sem_wait(&job_pool->sem);
}

void sync_job_prepare(sync_job_t *job, rain1_group_t *rxg,
                      bool rebuild, const slot_t *slot,
                      uint64_t su, uint64_t part)
{
    unsigned int i, nb_src, nb_dst;
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;

    /* Find the physical locations */
    rain1_slot_raw2rdev(rxg, slot, su * rxg->su_size, rdev_loc, &nb_rdev_loc, 3);

    nb_src = 0;
    nb_dst = 0;
    /* Identify sources and destinations */
    for (i = 0; i < nb_rdev_loc; i++)
    {
        rain1_realdev_t *lr = NULL;

        if (nb_src == 0 && rain1_rdev_location_readable(&rdev_loc[i]))
        {
            job->src_rdev_loc = rdev_loc[i];
            nb_src++;
            continue;
        }

        lr = RAIN1_REALDEV(rxg, rdev_loc[i].rdev);
        if (!rebuild ||
            (lr->mine && rain1_rdev_is_rebuilding(lr)
             && !rdev_loc[i].uptodate))
        {
            EXA_ASSERT(!lr->rebuild_progress.complete);
            job->dst_rdev_loc[nb_dst] = rdev_loc[i];
            nb_dst++;
        }
    }

    EXA_ASSERT(nb_src == 1 && nb_dst > 0);

    job->nb_dst         = nb_dst;
    job->nb_dst_written = 0;
    job->su             = su;
    job->part           = part;
}

int sync_job_lock(sync_job_t *job, uint64_t blksize)
{
    int ret;

    /* Lock the destination on the NBD if needed, before running
       the read on the source */

    /* FIXME: It now appears clealy that only the first destination is locked
     * I think we should take a lock on ALL the destinations AND on the source.
     */

    job->locked_rd = job->dst_rdev_loc[0].rdev;
    job->locked_rd_sector_start = job->dst_rdev_loc[0].sector + job->part * blksize;

    ret = vrt_rdev_lock_sectors(job->locked_rd, job->locked_rd_sector_start,
                                blksize);
    if (ret != EXA_SUCCESS)
    {
        job->locked_rd = NULL;
        job->error     = ret;
        job->completed = true;
        job->active    = false;
    }

    return ret;
}

int sync_job_unlock(sync_job_t *job, uint64_t blksize)
{
    /* Unlock previous round */
    if (job->locked_rd)
    {
        int ret = vrt_rdev_unlock_sectors(job->locked_rd, job->locked_rd_sector_start,
                                          blksize);
        if (ret != EXA_SUCCESS)
        {
            job->error  = ret;
            job->active = false;
            return ret;
        }

        job->locked_rd = NULL;
        job->locked_rd_sector_start = 0;
    }

    return EXA_SUCCESS;
}

static void sync_job_r_endio(blockdevice_io_t *bio, int error)
{
    sync_job_t *job = bio->private_data;

    job->error = error;

    job->step = SYNC_JOB_WRITE;
    job->completed = true;

    sync_job_signal_step_completed(job);
}

static void sync_job_w_endio(blockdevice_io_t *bio, int error)
{
    sync_job_t *job = bio->private_data;

    job->nb_dst_written++;

    job->error = error;

    if (job->nb_dst_written == job->nb_dst)
        job->step = SYNC_JOB_UNLOCK;

    job->completed = true;

    sync_job_signal_step_completed(job);
}

void sync_job_read(sync_job_t *job, uint64_t blksize)
{
    job->completed = false;

    __blockdevice_submit_io(job->src_rdev_loc.rdev->blockdevice, &job->bio,
                            BLOCKDEVICE_IO_READ,
                            job->src_rdev_loc.sector + job->part * blksize,
                            job->buffer, SECTORS_TO_BYTES(blksize), false,
                            true /* bypass_lock */, job, sync_job_r_endio);
}

void sync_job_write(sync_job_t *job, uint64_t blksize)
{
    EXA_ASSERT(job->nb_dst_written < job->nb_dst);
    job->completed = false;

    __blockdevice_submit_io(job->dst_rdev_loc[job->nb_dst_written].rdev->blockdevice,
                            &job->bio, BLOCKDEVICE_IO_WRITE,
                            job->dst_rdev_loc[job->nb_dst_written].sector + job->part * blksize,
                            job->buffer, SECTORS_TO_BYTES(blksize), true,
                            true /* bypass_lock */, job, sync_job_w_endio);
}
