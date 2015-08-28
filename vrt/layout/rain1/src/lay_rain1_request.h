/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_REQUEST_H__
#define __RAIN1_REQUEST_H__

#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"

#include "vrt/layout/rain1/src/lay_rain1_desync_info.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"


vrt_req_status_t rain1_build_io_for_req(struct vrt_request *vrt_req);
void rain1_init_req (struct vrt_request *vrt_req);
void rain1_cancel_req (struct vrt_request *vrt_req);
void rain1_declare_io_needs(struct vrt_request *vrt_req,
                            unsigned int *io_count,
                            bool *sync_afterward);

void rain1_schedule_aggregated_metadata(slot_desync_info_t *block, bool failure);

#endif
