/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_math.h"

#include "log/include/log.h"

#include "os/include/os_atomic.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"

#include "vrt/common/include/waitqueue.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_stats.h"
#include "vrt/virtualiseur/include/vrt_volume.h"

#include "vrt/virtualiseur/include/volume_blockdevice.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"

#define VOLUME_WIPE_SIZE (1024*1024) /* in bytes */

/**
 * Initialize the non persistent attributes of a volume
 *
 * @param[out] volume Volume to initialize
 */
static void vrt_volume_init(struct vrt_volume *volume)
{
    /* FIXME What about 'frozen'? */
    volume->status = EXA_VOLUME_STOPPED;
    init_waitqueue_head(&volume->frozen_req_wq);
    init_waitqueue_head(&volume->cmd_wq);
    volume->barrier_bio = NULL;
    os_thread_mutex_init(&volume->barrier_lock);
    init_waitqueue_head(&volume->barrier_req_wq);
    init_waitqueue_head(&volume->barrier_post_req_wq);
    os_atomic_set (& volume->inprogress_request_count, 0);
}

struct vrt_volume *vrt_volume_alloc(const exa_uuid_t *uuid, const char *name,
                                    uint64_t size)
{
    struct vrt_volume *vol;

    vol = os_malloc(sizeof(struct vrt_volume));
    if (vol == NULL)
        return NULL;

    /* FIXME It was done this way before, but we should initialize
             explicitely all fields of the volume */
    memset(vol, 0, sizeof(struct vrt_volume));

    uuid_copy(&vol->uuid, uuid);
    os_strlcpy(vol->name, name, sizeof(vol->name));
    vol->size = size;
    vol->group = NULL;
    vol->assembly_volume = NULL;

    /* FIXME Move contents of this useless function here */
    vrt_volume_init(vol);

    return vol;
}

/**
 * Free the vrt_volume struct
 */
void
__vrt_volume_free(struct vrt_volume *volume)
{
    os_free(volume);
}

bool vrt_volume_equals(const vrt_volume_t *a, const vrt_volume_t *b)
{
    if (!uuid_is_equal(&a->uuid, &b->uuid))
        return false;

    if (strcmp(a->name, b->name) != 0)
        return false;

    if (a->size != b->size)
        return false;

    /* FIXME We should test group equality, but that would cause
     * vrt_group_equals() to go into infinite recursive calls, because
     * vrt_group_equals() checks for its of its volumes equality.
     */

    return assembly_volume_equals(a->assembly_volume, b->assembly_volume);
}

static void vrt_volume_init_stats(vrt_volume_t *volume)
{
    memset(&volume->stats.begin, 0, sizeof(volume->stats.begin));
    memset(&volume->stats.done, 0, sizeof(volume->stats.done));

    /* Ensure a value different from both READ, WRITE and WRITE_BARRIER,
     * so that the first request is not counted as a seek. */
    volume->stats.begin.prev_request_type = VRT_IO_TYPE_NONE;

    volume->stats.last_reset = 0;
}

void vrt_volume_reset_stats(vrt_volume_t *volume)
{
    vrt_volume_init_stats(volume);
    volume->stats.last_reset = os_gettimeofday_msec();
}

/**
 * Start a volume, so that we can use its storage.
 *
 * @param[in] volume The volume to start
 *
 * @return EXA_SUCCESS on success, an error code on failure.
 */
int
vrt_volume_start(struct vrt_volume *volume)
{
    EXA_ASSERT (volume);

    if (volume->status == EXA_VOLUME_STARTED)
	return -VRT_ERR_VOLUME_ALREADY_STARTED;

    vrt_volume_init_stats(volume);

    /* Volume must be completely ready (i.e, started) before registering
       it in the kernel because automagic things such as udev can use it
       as soon as it becomes available. */
    volume->status = EXA_VOLUME_STARTED;

    return EXA_SUCCESS;
}

int vrt_volume_stop(struct vrt_volume *volume)
{
    EXA_ASSERT(volume != NULL);

    volume->status = EXA_VOLUME_STOPPED;

    return EXA_SUCCESS;
}

/**
 * Resize (either grow or shrink) a volume to a given new size.
 *
 * @param[in] volume     The volume to resize
 * @param[in] newsize    The new size of the volume in sectors (can be
 *                       lower or bigger than the current volume size)
 * @param[in] storage    The underlying storage
 *
 * @return EXA_SUCCESS on success, an error code on failure.
 */
int vrt_volume_resize(struct vrt_volume *volume, const uint64_t newsize,
                      const storage_t *storage)
{
    int ret;

    /* FIXME: the IO resquests issued before can cause
       an oops because of a newly freed slot */
    ret = volume->group->layout->volume_resize(volume, newsize, storage);

    if (ret == EXA_SUCCESS)
        volume->size = newsize;

    return ret;
}

/**
 * Get the admind status (UP/DOWN) of a volume.
 *
 * @param[in] volume     The volume
 *
 * @return VRT_ADMIND_STATUS_DOWN, VR_ADMIND_STATUS_UP,
 *         or an error code on failure.
 */
int
vrt_volume_get_status(const struct vrt_volume *volume)
{
    return volume->group->layout->volume_get_status(volume);
}

static int wipe_sectors(blockdevice_t *volume_handle, uint64_t first_sector,
                        int num_sectors)
{
    char *buffer = NULL;
    size_t buffer_size = SECTORS_TO_BYTES(num_sectors);
    int err;

    /* prepare wiping buffer */
    buffer = os_malloc(buffer_size);

    if (buffer == NULL)
        return -ENOMEM;

    memset(buffer, 0, buffer_size);

    err = blockdevice_write(volume_handle, buffer, buffer_size, first_sector);

    os_free(buffer);

    return err;
}

int vrt_volume_wipe(struct vrt_volume *volume)
{
    blockdevice_t *volume_handle = NULL;
    uint64_t n_sectors, first_sectors, last_sectors;
    int err;
    uint64_t wipe_size = MIN(VOLUME_WIPE_SIZE, SECTORS_TO_BYTES(volume->size));

    volume_handle = vrt_volume_create_block_device(volume, BLOCKDEVICE_ACCESS_RW);
    if (volume_handle == NULL)
    {
        exalog_error("Failed opening volume " UUID_FMT " for wiping.", UUID_VAL(&volume->uuid));
        return -ENOMEM;
    }

    n_sectors = BYTES_TO_SECTORS(wipe_size);
    first_sectors = 0;
    last_sectors = blockdevice_get_sector_count(volume_handle) - n_sectors;

    err = wipe_sectors(volume_handle, first_sectors, n_sectors);
    if (err == 0)
        err = wipe_sectors(volume_handle, last_sectors, n_sectors);

    if (err != 0)
        exalog_error("Failed wiping volume " UUID_FMT ": %s (%d).",
                     UUID_VAL(&volume->uuid), exa_error_msg(err), err);

    blockdevice_close(volume_handle);

    return err;
}

blockdevice_t *vrt_volume_create_block_device(vrt_volume_t *volume,
                                              blockdevice_access_t access)
{
    blockdevice_t *blockdevice = NULL;
    int err;

    err = volume_blockdevice_open(&blockdevice, volume, access);
    if (err != 0)
        return NULL; /* FIXME error eating */

    return blockdevice;
}

int vrt_volume_header_read(volume_header_t *header, stream_t *stream)
{
    int r;

    r = stream_read(stream, header, sizeof(volume_header_t));
    if (r < 0)
        return r;
    else if (r != sizeof(volume_header_t))
        return -EIO;

    return 0;
}

uint64_t vrt_volume_serialized_size(const vrt_volume_t *volume)
{
    return sizeof(volume_header_t);
}

int vrt_volume_serialize(const vrt_volume_t *volume, stream_t *stream)
{
    volume_header_t header;
    int w;

    header.magic = VOLUME_HEADER_MAGIC;
    header.reserved = 0;
    header.uuid = volume->uuid;
    os_strlcpy(header.name, volume->name, sizeof(header.name));
    header.size = volume->size;
    header.assembly_volume_uuid = volume->assembly_volume->uuid;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    return 0;
}

int vrt_volume_deserialize(vrt_volume_t **volume, const assembly_group_t *ag,
                              stream_t *stream)
{
    volume_header_t header;
    assembly_volume_t *av;
    int err;

    err = vrt_volume_header_read(&header, stream);
    if (err != 0)
        return err;

    if (header.magic != VOLUME_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    if (header.reserved != 0)
        return -VRT_ERR_SB_CORRUPTION;

    *volume = vrt_volume_alloc(&header.uuid, header.name, header.size);
    if (*volume == NULL)
        return -ENOMEM;

    av = assembly_group_lookup_volume(ag, &header.assembly_volume_uuid);
    if (av == NULL)
        return -VRT_ERR_SB_CORRUPTION;

    (*volume)->assembly_volume = av;

    return 0;
}
