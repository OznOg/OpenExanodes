/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_request.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"

#include "vrt/layout/rain1/src/lay_rain1_check.h"
#include "vrt/layout/rain1/src/lay_rain1_striping.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

#include "common/include/exa_error.h"

#include "log/include/log.h"


#define CHECK_NB_JOBS 64
#define CHECK_NB_LOC 3

typedef struct{
    struct rdev_location rdev_loc;
    bool is_accessible;
    blockdevice_io_t bio;
    completion_t io_completion;
    char *buffer;
    size_t buffer_size;
} replica_info_t;

typedef struct{
    size_t size;
    exa_uuid_t subspace_uuid;
    uint32_t slot_index;
    uint64_t sector_offset;
    unsigned int nb_replicas;
    replica_info_t replicas[CHECK_NB_LOC];
} check_block_job_t;

static void job_cleanup(check_block_job_t *job)
{
    unsigned int i;

    for (i = 0; i < CHECK_NB_LOC; i++)
    {
        os_free(job->replicas[i].buffer);
        job->replicas[i].buffer_size = 0;
    }
}

static int job_init(check_block_job_t *job, size_t size)
{
    unsigned int i;

    for (i = 0; i < CHECK_NB_LOC; i++)
    {
        job->replicas[i].buffer = os_malloc(size);
        job->replicas[i].buffer_size = size;

        if (job->replicas[i].buffer == NULL)
        {
            job_cleanup(job);
            return -ENOMEM;
        }
    }

    job->nb_replicas = 0;

    return EXA_SUCCESS;
}

static void job_init_replicas(rain1_group_t *rxg,
                              assembly_volume_t *subspace,
                              check_block_job_t *job,
                              uint64_t slot_index,
                              uint64_t sector_offset,
                              uint64_t size)
{
    unsigned int i;
    struct rdev_location rdev_loc[CHECK_NB_LOC];

    uuid_copy(&job->subspace_uuid, &subspace->uuid);
    job->slot_index = slot_index;
    job->sector_offset = sector_offset;
    job->size = size;

    os_thread_rwlock_rdlock(&rxg->status_lock);
    rain1_slot_data2rdev(rxg, subspace->slots[slot_index],
                         sector_offset,
                         rdev_loc, &job->nb_replicas, CHECK_NB_LOC);
    os_thread_rwlock_unlock(&rxg->status_lock);

    for (i = 0; i < job->nb_replicas; i++)
    {
        replica_info_t *replica = &job->replicas[i];

        EXA_ASSERT(replica->buffer_size >= job->size);
        EXA_ASSERT(replica->buffer != 0);

        replica->is_accessible = false;
        replica->rdev_loc = rdev_loc[i];
    }
}

static void job_io_complete(blockdevice_io_t *io, int error)
{
    complete((completion_t *)io->private_data, error);
}

static int job_write_replicas(check_block_job_t *job, uint64_t reset_value)
{
    unsigned int i;

    for (i = 0; i < job->nb_replicas; i++)
    {
        replica_info_t *replica = &job->replicas[i];

        init_completion(&replica->io_completion);

        memset(replica->buffer, reset_value, job->size);

        blockdevice_submit_io(replica->rdev_loc.rdev->blockdevice, &replica->bio,
                              BLOCKDEVICE_IO_WRITE, replica->rdev_loc.sector,
                              replica->buffer, job->size, false,
                              &replica->io_completion, job_io_complete);

        replica->is_accessible = true;
    }

    return EXA_SUCCESS;
}

static int job_read_replicas(check_block_job_t *job)
{
    unsigned int i;
    unsigned int nb_readable_loc = 0;

    for (i = 0; i < job->nb_replicas; i++)
    {
        replica_info_t *replica = &job->replicas[i];

        if (!rain1_rdev_location_readable(&replica->rdev_loc))
            continue;

        nb_readable_loc++;

        init_completion(&replica->io_completion);

        blockdevice_submit_io(replica->rdev_loc.rdev->blockdevice, &replica->bio,
                              BLOCKDEVICE_IO_READ, replica->rdev_loc.sector,
                              replica->buffer, job->size, false,
                              &replica->io_completion, job_io_complete);

        replica->is_accessible = true;
    }

    return nb_readable_loc > 0 ? EXA_SUCCESS : -EIO;
}

static int job_wait_completion(check_block_job_t *job)
{
    unsigned int i;
    int err = 0;

    for (i = 0; i < job->nb_replicas; i++)
    {
        replica_info_t *replica = &job->replicas[i];
        int local_err;

        if (!replica->is_accessible)
            continue;

        local_err = wait_for_completion(&replica->io_completion);
        if (local_err != 0)
            err = local_err;
    }

    return err;
}

static int job_check_replicas(check_block_job_t *job)
{
    unsigned int i;
    replica_info_t *first_replica = NULL;

    for (i = 0; i < job->nb_replicas; i++)
    {
        replica_info_t *replica = &job->replicas[i];

        if (!replica->is_accessible)
            continue;

        if(first_replica == NULL)
        {
            first_replica = replica;
            continue;
        }

        if (memcmp(first_replica->buffer, replica->buffer, job->size))
        {
            exalog_error("Inconsistent data at logical sector %"PRIu64" of slot %"PRIu32
                         " subspace " UUID_FMT " between first and %u th replica"
                         " (nb_replicas=%d)",
                         job->sector_offset, job->slot_index,
                         UUID_VAL(&job->subspace_uuid),
                         i, job->nb_replicas);

            exalog_error("First replica rdev " UUID_FMT " sector %"PRIu64" : 0x%" PRIu64,
                         UUID_VAL(&first_replica->rdev_loc.rdev->uuid),
                         first_replica->rdev_loc.sector,
                         *(uint64_t*)first_replica->buffer);

            exalog_error("Other replica rdev " UUID_FMT " sector %"PRIu64" : 0x%" PRIu64,
                         UUID_VAL(&replica->rdev_loc.rdev->uuid),
                         replica->rdev_loc.sector,
                         *(uint64_t*)replica->buffer);

            return -EIO;
        }
    }

    return EXA_SUCCESS;
}

static int rain1_group_reset_slot(rain1_group_t *lg,
                                  assembly_volume_t *subspace,
                                  uint32_t slot_index)
{
    uint32_t nb_sectors_per_block = lg->max_sectors;
    check_block_job_t jobs[CHECK_NB_JOBS];
    uint64_t logical_slot_size = rain1_group_get_slot_data_size(lg);
    unsigned int sector_offset;
    unsigned int i;
    int ret = EXA_SUCCESS;

    for (i = 0; i < CHECK_NB_JOBS; i++)
    {
        ret = job_init(&jobs[i], SECTORS_TO_BYTES(nb_sectors_per_block));

        if (ret != EXA_SUCCESS)
            goto out;
    }

    sector_offset = 0;
    while (sector_offset < logical_slot_size)
    {
        unsigned int nb_job_initialized = 0;

        for (i = 0; i < CHECK_NB_JOBS; i++)
        {
            check_block_job_t *job = &jobs[i];
            size_t sector_size;

            if (sector_offset >= logical_slot_size)
                break;

            sector_size = MIN(nb_sectors_per_block, logical_slot_size - sector_offset);

            job_init_replicas(lg, subspace, job, slot_index, sector_offset,
                              SECTORS_TO_BYTES(sector_size));

            ret = job_write_replicas(job, sector_offset);

            if (ret != EXA_SUCCESS)
                goto out;

            sector_offset += sector_size;
            nb_job_initialized++;
        }

        for (i = 0; i < nb_job_initialized; i++)
        {
            ret = job_wait_completion(&jobs[i]);

            if (ret != EXA_SUCCESS)
                goto out;
        }
    }

out:
    for (i = 0; i < CHECK_NB_JOBS; i++)
        job_cleanup(&jobs[i]);

    return ret;
}


static int rain1_group_check_slot(rain1_group_t *lg,
                                  assembly_volume_t *subspace,
                                  uint32_t slot_index)
{
    uint32_t nb_sectors_per_block = lg->max_sectors;
    check_block_job_t jobs[CHECK_NB_JOBS];
    uint64_t logical_slot_size = rain1_group_get_slot_data_size(lg);
    unsigned int sector_offset;
    unsigned int i;
    int ret = EXA_SUCCESS;

    for (i = 0; i < CHECK_NB_JOBS; i++)
    {
        ret = job_init(&jobs[i], SECTORS_TO_BYTES(nb_sectors_per_block));

        if (ret != EXA_SUCCESS)
            goto out;
    }

    sector_offset = 0;
    while (sector_offset < logical_slot_size)
    {
        unsigned int nb_job_initialized = 0;

        for (i = 0; i < CHECK_NB_JOBS; i++)
        {
            check_block_job_t *job = &jobs[i];
            unsigned int sector_size;

            if (sector_offset >= logical_slot_size)
                break;

            sector_size = MIN(nb_sectors_per_block, logical_slot_size - sector_offset);

            job_init_replicas(lg, subspace, job, slot_index, sector_offset,
                              SECTORS_TO_BYTES(sector_size));

            ret = job_read_replicas(job);

            if (ret != EXA_SUCCESS)
                goto out;

            sector_offset += sector_size;
            nb_job_initialized++;
        }

        for (i = 0; i < nb_job_initialized; i++)
        {
            ret = job_wait_completion(&jobs[i]);

            if (ret != EXA_SUCCESS)
                goto out;
        }

        for (i = 0; i < nb_job_initialized; i++)
        {
            ret = job_check_replicas(&jobs[i]);

            if (ret != EXA_SUCCESS)
                goto out;
        }
    }

out:
    for (i = 0; i < CHECK_NB_JOBS; i++)
        job_cleanup(&jobs[i]);

    return ret;
}

static int rain1_group_reset_subspace(rain1_group_t *lg, assembly_volume_t *subspace)
{
    uint64_t slot_index;

    for (slot_index = 0; slot_index < subspace->total_slots_count; slot_index++)
    {
	int ret;

        /* Each slot is reset by a different node */
	if ((slot_index % vrt_node_get_upnodes_count()) != vrt_node_get_upnode_id())
	    continue;

        ret = rain1_group_reset_slot(lg, subspace, slot_index);

        if (ret != EXA_SUCCESS)
        {
            exalog_debug("Failed to reset slot %"PRIu64" in subspace "UUID_FMT
                         ": %s (%d)", slot_index, UUID_VAL(&subspace->uuid),
                         exa_error_msg(ret), ret);
            return ret;
        }
    }
    return EXA_SUCCESS;
}

int rain1_group_reset(void *private_data)
{
    rain1_group_t *lg = (rain1_group_t *)private_data;
    assembly_volume_t *subspace;

    /*
     * Reset the group subspace by subspace
     */
    for (subspace = lg->assembly_group.subspaces; subspace != NULL;
         subspace = subspace->next)
    {
        int err = rain1_group_reset_subspace(lg, subspace);
        if (err != EXA_SUCCESS)
        {
            exalog_error("Failed to reset subspace "UUID_FMT": %s (%d)",
                         UUID_VAL(&subspace->uuid), exa_error_msg(err), err);
            return err;
        }
    }

    return EXA_SUCCESS;
}

static int rain1_group_check_subspace(rain1_group_t *lg, assembly_volume_t *subspace)
{
    uint64_t slot_index;

    for (slot_index = 0; slot_index < subspace->total_slots_count; slot_index++)
    {
        int ret;

	/* Each slot is checked by a different node */
	if ((slot_index % vrt_node_get_upnodes_count()) != vrt_node_get_upnode_id())
	    continue;

        ret = rain1_group_check_slot(lg, subspace, slot_index);
        if (ret != EXA_SUCCESS)
        {
            exalog_error("Failed to check slot %"PRIu64" in subspace "UUID_FMT
                         ": %s (%d)", slot_index, UUID_VAL(&subspace->uuid),
                         exa_error_msg(ret), ret);
            return ret;
        }
    }

    return EXA_SUCCESS;
}

int rain1_group_check(void *private_data)
{
    rain1_group_t *lg = (rain1_group_t *)private_data;
    assembly_volume_t *subspace;

    /*
     * Check the group subspace by subspace
     */
    for (subspace = lg->assembly_group.subspaces; subspace != NULL;
         subspace = subspace->next)
    {
        int err = rain1_group_check_subspace(lg, subspace);
        if (err != EXA_SUCCESS)
        {
            exalog_error("Failed to check subspace "UUID_FMT": %s (%d)",
                         UUID_VAL(&subspace->uuid), exa_error_msg(err), err);
            return err;
        }
    }

    return EXA_SUCCESS;
}

