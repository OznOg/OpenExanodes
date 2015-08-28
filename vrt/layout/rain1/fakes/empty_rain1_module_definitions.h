/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_RAIN1_MODULE_FAKES_H__
#define __UT_RAIN1_MODULE_FAKES_H__

#include "vrt/layout/rain1/src/lay_rain1_request.h"

int rain1_group_resync(struct vrt_group *group)
{
    return 0;
}
int rain1_group_rebuild(struct vrt_group *group)
{
    return 0;
}
vrt_req_status_t rain1_build_io_for_req(struct vrt_request *vrt_req)
{
    return 0;
}

void rain1_set_rebuilding_slowdown(int _rebuilding_slowdown_ms,
                                   int _degraded_rebuilding_slowdown_ms)
{}

void rain1_init_req (struct vrt_request *vrt_req)
{
}
void rain1_cancel_req (struct vrt_request *vrt_req)
{
}
void rain1_declare_io_needs(struct vrt_request *vrt_req,
                                unsigned int *io_count,
                                bool *sync_afterward)
{
}
void rain1_schedule_aggregated_metadata(slot_desync_info_t *block,
                                        int failure)
{
}
#endif
