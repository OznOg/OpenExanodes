/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_REQUEST_H__
#define __VRT_REQUEST_H__

#include "vrt/common/include/list.h"
#include "os/include/os_atomic.h"

#include "vrt/virtualiseur/include/vrt_volume.h"

typedef struct vrt_request vrt_request_t;

/**
 * The stucture IO is used as a list of IO to perform. It is a link
 * list created by the layout in order to tell the vrt engine to
 * perform a list of IO before calling it back.
 */
struct vrt_io_op
{
    /** Type of I/O. Might be READ or WRITE */
    vrt_io_type_t iotype;

    /** The real device on which the I/O should be made */
    struct vrt_realdev *rdev;

    /** Status of the I/O */
    enum { IO_TO_PROCESS, IO_DONT_PROCESS, IO_OK, IO_FAILURE } state;

    /** Back-pointer to the request header in which this I/O is
	subscribed */
    vrt_request_t *vrt_req;

    /** The bio head corresponding to this I/O */
    blockdevice_io_t *bio;

    /** Offset of data on real device, in sector */
    uint64_t offset;

    /** Pointer to the data. */
    void *data;

    /** Size of the I/O. */
    uint32_t size;

#ifdef WITH_PERF
    /** Date of submition */
    uint64_t submit_date;
    /** Date of termination */
    uint64_t end_date;
#endif

    /** Next I/O in the I/O list of the vrt_request */
    struct vrt_io_op *next;
};

/**
 * This structure is a linked list of barriers filled by the layout and
 * performed by the VRT engine.
 */
struct vrt_barrier_op
{
    /** The number of remaining disks whose barrier is not yet completed */
    os_atomic_t remaining_disks;

    /** State of the barrier operation */
    enum {BARRIER_TO_PROCESS, BARRIER_DONT_PROCESS, BARRIER_OK, BARRIER_FAILURE} state;

    /** Back-pointer to the request header in which this barrier is subscribed */
    vrt_request_t *vrt_req;
};

/**
 * The structure vrt_request is the main structure that allows the
 * communication between the virtualizer and the layout. This
 * structure is allocated by the layout and is so deleteted by the
 * layout.
 *
 * When the virtualizer is asked to perform a request, it asks the
 * layout for such a structure, and then fills its fields before
 * giving it back to the layout in order to get a list of IO to
 * perform.
 */
struct vrt_request
{

    struct vrt_io_op *io_list;

    struct vrt_barrier_op *barrier;

    /** List head that can be used by the layout to link "forget" requests */
    struct list_head wait_list;

    /** List head that can be used by the layout to link "pending" (ask/suggest) requests */
    struct list_head req_pending_list;

    /** Next request to wake up when the "pending" request (ask/suggest) has finished */
    struct vrt_request *next_req_pending;

    /** Layout private data */
#define VRT_PRIVATE_DATA_SIZE (16)
    char private_data[VRT_PRIVATE_DATA_SIZE];

    /** The bio representing the initial request coming from the
	kernel */
    blockdevice_io_t *ref_bio;

    /** Type of I/O of the initial request coming from the kernel. Might be READ,
     * READA, WRITE or WRITE_BARRIER. */
    vrt_io_type_t iotype;

    /** The volume on which the request coming from the kernel takes place. */
    struct vrt_volume *ref_vol;

    /** The number of remaining I/O to perform or msg to receive */
    os_atomic_t remaining;

    /** Do not perfom IO before this date. Used for "replay request" feature. */
    uint64_t replay_date;

#ifdef WITH_PERF
    /** Number of operations done to treat the IO ... I suppose */
    uint32_t nb_io_ops;
#endif

    /** Request header might be part of a linked-list, in order to be
	processed later by a tasklet */
    struct vrt_request *next;
};

#define VRT_REQ_GET_GROUP(vrt_req) ((vrt_req)->ref_vol->group)

void vrt_wakeup_request(struct vrt_request *vrt_req);
void vrt_thread_wakeup(void);

int vrt_engine_init(int max_requests);
void vrt_engine_cleanup(void);
void vrt_make_request(void *private_data, blockdevice_io_t *bio);
unsigned int vrt_get_max_requests(void);

#endif /* __VRT_REQUEST_H__ */
