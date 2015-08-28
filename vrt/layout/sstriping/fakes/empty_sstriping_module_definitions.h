/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_SSTRIPING_MODULE_FAKES_H__
#define __UT_SSTRIPING_MODULE_FAKES_H__

int sstriping_build_io_for_req(void *vrt_req)
{
    return 0;
}

void sstriping_init_req (void *vrt_req)
{
}
void sstriping_cancel_req (void *vrt_req)
{
}
void sstriping_declare_io_needs(void *vrt_req,
                                unsigned int *io_count,
                                bool *sync_afterward)
{
}

#endif
