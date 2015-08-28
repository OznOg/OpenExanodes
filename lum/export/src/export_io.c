/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "lum/export/src/export_io.h"
#include "lum/export/include/executive_export.h"

#include "blockdevice/include/blockdevice.h"

#include "common/include/exa_nbd_list.h"
#include "common/include/exa_math.h"

#include <errno.h>

typedef struct
{
    lum_export_end_io_t *callback;
    void *caller_private_data;
    blockdevice_io_t bio;
} lum_export_io_private_t;

static struct
{
    struct nbd_root_list io_data;
} lum_pools;

/* 512 is legacy I do not know if the value is meaningful. */
#define NB_IO_IN_LUM 512

int export_io_static_init(void)
{
    nbd_init_root(NB_IO_IN_LUM, sizeof(lum_export_io_private_t),
                  &lum_pools.io_data);
    return EXA_SUCCESS;
}

void export_io_static_cleanup(void)
{
    nbd_close_root(&lum_pools.io_data);
}

static void lum_io_data_put(lum_export_io_private_t *io_data)
{
    nbd_list_post(&lum_pools.io_data.free, io_data, -1);
}

static lum_export_io_private_t *lum_io_data_alloc(void)
{
    lum_export_io_private_t *io_data;
    io_data = nbd_list_remove(&lum_pools.io_data.free, NULL, LISTWAIT);
    EXA_ASSERT(io_data != NULL);

    return io_data;
}

static void __end_io(blockdevice_io_t *bio, int err)
{
    lum_export_io_private_t *io_private = bio->private_data;
    lum_export_end_io_t *callback = io_private->callback;
    void *caller_private_data = io_private->caller_private_data;

    /* IO is finished, error code and data are retrieved, we can safely
     * release the bio */
    lum_io_data_put(io_private);

    callback(err, caller_private_data);
}

/**
 *  function for target adapter: submit an io to a lun
 *
 *  USED ONLY BY LUM
 *
 *  @param lun         Targeted lun
 *  @param op          BLOCKDEVICE_IO_READ or BLOCKDEVICE_IO_WRITE
 *  @param flush_cache The IO needs a disk cache synchronization (barrier)
 *  @param sector      First sector
 *  @param size        Size of the io (in bytes)
 *  @param buf         Address of the io buffer
 *  @param bi_private  Data for callback (accessible with bio_get_private())
 *  @param callback    Callback called at the end of the io
 */
void lum_export_submit_io(lum_export_t *export, blockdevice_io_type_t op, bool flush_cache,
                          long long sector, int size,
                          void *buf, void *bi_private,
                          lum_export_end_io_t *callback)
{
    lum_export_io_private_t *io_data;
    blockdevice_io_t *bio;

    EXA_ASSERT(export != NULL);
    EXA_ASSERT(BLOCKDEVICE_IO_TYPE_IS_VALID(op));

    if (op != BLOCKDEVICE_IO_READ && lum_export_is_readonly(export))
    {
        /* This is not supposed to happen in nominal use because bdev
         * and target are not supposed to send io if in read only.
         * But this cas still happen for example if the user sets the bdev
         * into rw mode with hdparm, so this code is an extra precaution. */
        callback(-EIO, bi_private);
        return;
    }
    io_data = lum_io_data_alloc();
    bio = &io_data->bio;

    io_data->caller_private_data = bi_private;
    io_data->callback            = callback;

    blockdevice_submit_io(lum_export_get_blockdevice(export), bio, op, sector, buf, size,
                            flush_cache, io_data, __end_io);
}
