/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_REQUEST_FAKES_H__
#define __UT_VRT_REQUEST_FAKES_H__

#include "vrt/virtualiseur/include/vrt_request.h"

void vrt_thread_wakeup(void)
{
}

int vrt_engine_init(int max_requests)
{
    return 0;
}
void vrt_engine_cleanup(void)
{
}
void vrt_make_request(void *private_data, blockdevice_io_t *bio)
{
}

unsigned int vrt_get_max_requests(void)
{
    return 0;
}

void vrt_wakeup_request(struct vrt_request *vrt_req)
{

}

#endif
