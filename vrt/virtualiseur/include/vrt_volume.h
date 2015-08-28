/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_VOLUME_H__
#define __VRT_VOLUME_H__

#include "common/include/uuid.h"

#include "os/include/os_atomic.h"
#include "os/include/os_thread.h"

#include "blockdevice/include/blockdevice.h"

#include "vrt/common/include/waitqueue.h"

#include "vrt/virtualiseur/include/vrt_common.h"
#include "vrt/virtualiseur/include/vrt_volume_stats.h"

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/assembly/src/assembly_volume.h"

/** Data structure used to represent a volume */
typedef struct vrt_volume
{
    /** The UUID that uniquely identifies this volume */
    exa_uuid_t uuid;

    /** The name of this volume */
    char name[EXA_MAXSIZE_VOLUMENAME + 1];

    /** The volume size presented to the upper layers (in sectors) */
    uint64_t size;

    /** Back pointer to the group to which this volume belongs */
    struct vrt_group *group;

    /** Status */
    exa_volume_status_t status;

    /* Assembly volume that contains the array of slot indexes */
    struct assembly_volume *assembly_volume;

    /** Statistics counters. */
    struct vrt_stats_volume stats;

    /** Set during a freeze (during a "reintegrate complete") */
    int frozen;

    /** Waitqueue on which processes waiting to access the volume are
	registered (when the volume is frozen) */
    wait_queue_head_t frozen_req_wq;

    /** Waitqueue on which the cmd thread will wait until all
	"initialized requests" are completed (until initialized_request_count,
	defined below, becomes 0). */
    wait_queue_head_t cmd_wq;

    /** Set when a barrier on the volume is ongoing. */
    blockdevice_io_t *barrier_bio;

    /** Mutex used to do an atomic test and set of barrier bio */
    os_thread_mutex_t barrier_lock;

    /** Waitqueue on which the task that requested the barrier is registered. */
    wait_queue_head_t barrier_req_wq;

    /** Waitqueue on which processes waiting to access the volume after
	a barrier are registered. */
    wait_queue_head_t barrier_post_req_wq;

    /** The number of request handled by the virtualizer that are in progress.
	It is used to be able to wait until all requests have finished. We do not
	count request of type "normal-sub". */
    os_atomic_t inprogress_request_count;
} vrt_volume_t;

struct vrt_volume *vrt_volume_alloc(const exa_uuid_t *uuid, const char *name,
                                    uint64_t size);
void __vrt_volume_free(struct vrt_volume *volume);
#define vrt_volume_free(vol) (__vrt_volume_free(vol), vol = NULL)

bool vrt_volume_equals(const vrt_volume_t *a, const vrt_volume_t *b);

int vrt_volume_start(struct vrt_volume *volume);
int vrt_volume_export(const char *buf, size_t buf_size);
int vrt_volume_unexport(struct vrt_volume *volume);
int vrt_volume_stop(struct vrt_volume *volume);

int vrt_volume_resize(struct vrt_volume *volume, const uint64_t newsize,
                      const storage_t *storage);
int vrt_volume_sync_sb(struct vrt_volume *volume, int position);
int vrt_volume_get_status(const struct vrt_volume *volume);

int vrt_volume_wipe(struct vrt_volume *volume);

blockdevice_t *vrt_volume_create_block_device(struct vrt_volume *volume,
                                              blockdevice_access_t access);

typedef enum { VOLUME_HEADER_MAGIC = 0x7700FF11 } volume_header_magic_t;

typedef struct
{
    volume_header_magic_t magic;
    uint32_t reserved; /**< padding with reserved field */
    exa_uuid_t uuid;
    char name[EXA_MAXSIZE_VOLUMENAME + 1];
    uint64_t size;
    exa_uuid_t assembly_volume_uuid;
    /* FIXME stats too? */
} volume_header_t;

int vrt_volume_header_read(volume_header_t *header, stream_t *stream);

uint64_t vrt_volume_serialized_size(const vrt_volume_t *volume);

int vrt_volume_serialize(const vrt_volume_t *volume, stream_t *stream);
int vrt_volume_deserialize(vrt_volume_t **volume, const assembly_group_t *ag,
                              stream_t *stream);

void vrt_volume_reset_stats(vrt_volume_t *volume);

#endif /* __VRT_VOLUME_H__ */
