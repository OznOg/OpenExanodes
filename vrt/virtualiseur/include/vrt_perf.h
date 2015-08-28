/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_PERF_H
#define __VRT_PERF_H

#include "vrt/virtualiseur/include/vrt_request.h" // FIXME bio + WRITE_BARRIER
#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"

#ifdef WITH_PERF
#define VRT_PERF_INIT()	vrt_perf_init()
#define VRT_PERF_DEBUG_BEGIN(i) vrt_perf_debug_begin(i)
#define VRT_PERF_DEBUG_END(i) vrt_perf_debug_end(i)
#define VRT_PERF_DEBUG_FLUSH() vrt_perf_debug_flush()
#define VRT_PERF_MAKE_REQUEST(io_type, bio)	vrt_perf_make_request(io_type, bio)
#define VRT_PERF_END_REQUEST(vrt_req)	vrt_perf_end_request(vrt_req)
#define VRT_PERF_IO_OP_SUBMIT(vrt_req, io_op) \
    vrt_perf_io_op_submit(vrt_req, io_op)
#define VRT_PERF_IO_OP_END(io_op) vrt_perf_io_op_end(io_op)
#define RAINX_PERF_RESYNC_SLOT_BEGIN() rainx_perf_resync_slot_begin()
#define RAINX_PERF_RESYNC_SLOT_END() rainx_perf_resync_slot_end()
#define RAINX_PERF_RESYNC_SLOT_FLUSH() rainx_perf_resync_slot_flush()
#define RAINX_PERF_POST_RESYNC_BEGIN() rainx_perf_post_resync_begin()
#define RAINX_PERF_POST_RESYNC_END() rainx_perf_post_resync_end()
#define RAINX_PERF_POST_RESYNC_FLUSH() rainx_perf_post_resync_flush()
#define RAINX_PERF_STOP_BEGIN() rainx_perf_stop_begin()
#define RAINX_PERF_STOP_END() rainx_perf_stop_end()
#define RAINX_PERF_STOP_FLUSH() rainx_perf_stop_flush()

#else
#define VRT_PERF_INIT()
#define VRT_PERF_DEBUG_BEGIN(i)
#define VRT_PERF_DEBUG_END(i)
#define VRT_PERF_DEBUG_FLUSH()
#define VRT_PERF_MAKE_REQUEST(io_type, bio)
#define VRT_PERF_END_REQUEST(vrt_req)
#define VRT_PERF_IO_OP_SUBMIT(vrt_req, io_op)
#define VRT_PERF_IO_OP_END(io_op)
#define RAINX_PERF_RESYNC_SLOT_BEGIN()
#define RAINX_PERF_RESYNC_SLOT_END()
#define RAINX_PERF_RESYNC_SLOT_FLUSH()
#define RAINX_PERF_POST_RESYNC_BEGIN()
#define RAINX_PERF_POST_RESYNC_END()
#define RAINX_PERF_POST_RESYNC_FLUSH()
#define RAINX_PERF_STOP_BEGIN()
#define RAINX_PERF_STOP_END()
#define RAINX_PERF_STOP_FLUSH()
#endif

void vrt_perf_init();

void vrt_perf_debug_begin(unsigned int num_debug);
void vrt_perf_debug_end(unsigned int num_debug);
void vrt_perf_debug_flush(void);

void vrt_perf_make_request(vrt_io_type_t io_type, blockdevice_io_t *bio);
void vrt_perf_end_request(struct vrt_request *vrt_req);
void vrt_perf_io_op_submit(struct vrt_request *vrt_req,
			   struct vrt_io_op *io_op);
void vrt_perf_io_op_end(struct vrt_io_op *io_op);
void rainx_perf_resync_slot_begin(void);
void rainx_perf_resync_slot_end(void);
void rainx_perf_resync_slot_flush(void);

void rainx_perf_post_resync_begin(void);
void rainx_perf_post_resync_end(void);
void rainx_perf_post_resync_flush(void);

void rainx_perf_stop_begin(void);
void rainx_perf_stop_end(void);
void rainx_perf_stop_flush(void);

#endif /* __VRT_PERF_H */
