/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */




#include "common/include/exa_error.h"
#include "os/include/os_atomic.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_request.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_volume.h"

#include "vrt/layout/sstriping/src/lay_sstriping.h"


/** State of a sstriping request */
typedef enum
{
    SSTRIPING_REQUEST_BEGIN,
    SSTRIPING_REQUEST_WRITE_BARRIER,
    SSTRIPING_REQUEST_END
} sstriping_request_state_t;

static void
sstriping_req_set_state(struct vrt_request *vrt_req, sstriping_request_state_t state)
{
    sstriping_request_state_t *dest_state;

    COMPILE_TIME_ASSERT(sizeof(state) <= VRT_PRIVATE_DATA_SIZE);

    dest_state = (sstriping_request_state_t *) vrt_req->private_data;
    *dest_state = state;
}

static sstriping_request_state_t
sstriping_req_get_state(const struct vrt_request *vrt_req)
{
    return *(sstriping_request_state_t*)(vrt_req->private_data);
}

/**
 * This function fills the list of IOs given by the VRT engine
 * according to the sstriping data placement policy.
 *
 * @param[in] vrt_req The descriptor of the original request.
 */
static vrt_req_status_t
sstriping_fill_io(struct vrt_request *vrt_req)
{
    uint64_t sec_rd;
    vrt_realdev_t *rd;
    struct vrt_io_op *io;

    EXA_ASSERT (vrt_req != NULL);

    /* Initialize all vrt_io_op to IO_DONT_PROCESS */
    for (io = vrt_req->io_list ; io != NULL ; io = io->next)
	io->state = IO_DONT_PROCESS;

    /* Convert position in the virtual device into a position in a
       disk */
    sstriping_volume2rdev(vrt_req->ref_vol, vrt_req->ref_bio->start_sector,
			  & rd, & sec_rd);

    io = vrt_req->io_list;

    /* There should be only one IO in the list */
    EXA_ASSERT (io != NULL);
    EXA_ASSERT (io->next == NULL);

    io->iotype  = vrt_req->iotype;
    if (!rdev_is_ok(rd))
	io->state = IO_DONT_PROCESS;
    else
	io->state = IO_TO_PROCESS;
    io->data    = vrt_req->ref_bio->buf;
    io->size    = vrt_req->ref_bio->size;
    io->vrt_req = vrt_req;
    io->rdev    = rd;
    io->offset  = sec_rd;

    if (io->state == IO_DONT_PROCESS)
	return VRT_REQ_FAILED;
    else
	return VRT_REQ_UNCOMPLETED;
}

/**
 * Fill a volume barrier, that must be issued on all writable disks
 * that are part of the group *before* issuing the write flagged as
 * BIO_RW_BARRIER. It is mandatory to issue this barrier on *all*
 * disks because it is the only way to ensure that all previous write
 * requests are stored on non-volatile storage.
 *
 * @param[out] vrt_req The request header
 *
 * @return VRT_REQ_UNCOMPLETED
 */
static vrt_req_status_t
sstriping_fill_barrier(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;

    EXA_ASSERT(vrt_req->iotype == VRT_IO_TYPE_WRITE_BARRIER);

    for (io = vrt_req->io_list; io != NULL; io = io->next)
	io->state = IO_DONT_PROCESS;

    vrt_req->barrier->state = BARRIER_TO_PROCESS;

    return VRT_REQ_UNCOMPLETED;
}

/**
 * Function called by the virtualizer to fill the list of I/O to
 * process in order to perform the request described by the given
 * vrt_request. This function is called several times, because the
 * layout may need several successive rounds of I/O requests, or
 * because of errors.
 *
 * This function can use the private_data field of the vrt_request to
 * store information that will allow it to know at which step of the
 * process it is.
 *
 * @param vrt_req The request header
 */
vrt_req_status_t
sstriping_build_io_for_req(struct vrt_request *vrt_req)
{
    sstriping_request_state_t state;
    vrt_req_status_t ret;

    state = sstriping_req_get_state(vrt_req);

    if (state == SSTRIPING_REQUEST_BEGIN)
    {
	if (vrt_req->iotype == VRT_IO_TYPE_WRITE_BARRIER)
	{
	    ret = sstriping_fill_barrier(vrt_req);
	    state = SSTRIPING_REQUEST_WRITE_BARRIER;
	}
	else
	{
	    ret = sstriping_fill_io(vrt_req);
	    state = SSTRIPING_REQUEST_END;
	}
    }
    else if (state == SSTRIPING_REQUEST_WRITE_BARRIER)
    {
        EXA_ASSERT(vrt_req->barrier != NULL);
        if (vrt_req->barrier->state == BARRIER_OK)
        {
            ret = sstriping_fill_io(vrt_req);
            state = SSTRIPING_REQUEST_END;
        }
        else
            ret = VRT_REQ_FAILED;
    }
    else if (state == SSTRIPING_REQUEST_END)
    {
	EXA_ASSERT(vrt_req->io_list->state == IO_OK ||
		   vrt_req->io_list->state == IO_FAILURE);
	if (vrt_req->io_list->state == IO_FAILURE)
	    ret = VRT_REQ_FAILED;
	else
	    ret = VRT_REQ_SUCCESS;
    }
    else
    {
	ret = VRT_REQ_FAILED;
	EXA_ASSERT_VERBOSE(FALSE, "Unknown sstriping request state %d.\n", state);
    }

    /* Update state */
    sstriping_req_set_state(vrt_req, state);

    return ret;
}

/**
 * Initialize a request
 *
 * @param[in] vrt_req The request
 */
void
sstriping_init_req (struct vrt_request *vrt_req)
{
    sstriping_req_set_state(vrt_req, SSTRIPING_REQUEST_BEGIN);
}

/**
 * Cancel a request. In this layout, it simply does the same as what
 * sstriping_init_req() does.
 *
 * @param[in] vrt_req The request
 */
void
sstriping_cancel_req (struct vrt_request *vrt_req)
{
    sstriping_req_set_state(vrt_req, SSTRIPING_REQUEST_BEGIN);
}

/**
 * Function called by the virtualizer to know what are the needs for
 * IO structure of the layout. It is called once for each request
 * received by the VRT engine.
 *
 * In this simple striping layout, we need one IO for a READ or WRITE
 * request. If the request type is WRITE_BARRIER, we also need a
 * barrier.
 */
void sstriping_declare_io_needs(struct vrt_request *vrt_req,
                                unsigned int *io_count,
                                bool *sync_afterward)
{
    EXA_ASSERT (vrt_req != NULL);
    EXA_ASSERT (io_count != NULL);
    EXA_ASSERT (sync_afterward != NULL);

    *io_count  = 1;
    if (vrt_req->iotype == VRT_IO_TYPE_WRITE_BARRIER)
	*sync_afterward = true;
    else
	*sync_afterward = false;
}
