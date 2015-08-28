/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memset */

#include "vrt/virtualiseur/fakes/fake_rdev.h"

#include "blockdevice/include/blockdevice.h"

#include "common/include/exa_error.h"

#include "os/include/os_assert.h"
#include "os/include/os_mem.h"


static const char *dummy_get_name(const void *context)
{
    return NULL;
}

static uint64_t dummy_get_sector_count(const void *context)
{
    const uint64_t *a = context;

    return *a;
}

static int dummy_set_sector_count(void *context, uint64_t count)
{
    return -EPERM;
}

static int dummy_submit_io(void *context, blockdevice_io_t *io)
{
    blockdevice_end_io(io, 0);
    return 0;
}

static int dummy_close(void *context)
{
    os_free(context);
    return 0;
}

static blockdevice_ops_t dummy_bdev_ops =
{
    .get_name_op = dummy_get_name,

    .get_sector_count_op = dummy_get_sector_count,
    .set_sector_count_op = dummy_set_sector_count,

    .submit_io_op = dummy_submit_io,

    .close_op = dummy_close
};

blockdevice_t *make_fake_blockdevice(uint64_t sector_count)
{
    blockdevice_t *bd;
    uint64_t *dummy_bdev_ctx = os_malloc(sizeof(uint64_t));

    *dummy_bdev_ctx = sector_count;

    blockdevice_open(&bd, dummy_bdev_ctx, &dummy_bdev_ops,
                     BLOCKDEVICE_ACCESS_RW);

    return bd;
}

struct vrt_realdev *make_fake_rdev(exa_nodeid_t node_id, spof_id_t spof_id,
                                   const exa_uuid_t *uuid,
                                   const exa_uuid_t *nbd_uuid,
                                   uint64_t real_size,
                                   int local, bool up)
{
    struct vrt_realdev *rdev;

    /* FIXME Should have vrt_realdev_alloc() */
    rdev = os_malloc(sizeof(struct vrt_realdev));
    if (rdev == NULL)
	return NULL;

    memset(rdev, 0, sizeof(struct vrt_realdev));

    rdev->real_size = real_size;

    rdev->local = local;

    rdev->node_id = node_id;
    rdev->spof_id = spof_id;
    rdev->index   = 0;

    rdev->up = up;
    rdev->corrupted = FALSE;

    rdev->blockdevice = make_fake_blockdevice(BYTES_TO_SECTORS(real_size));
    if (rdev->blockdevice == NULL)
        return NULL;

    uuid_copy(&rdev->uuid, uuid);
    uuid_copy(&rdev->nbd_uuid, nbd_uuid);

    rdev->chunks.chunk_size = 0;
    rdev->chunks.total_chunks_count = 0;
    rdev->chunks.free_chunks_count = 0;
    rdev->chunks.free_chunks = NULL;

    return rdev;
}

