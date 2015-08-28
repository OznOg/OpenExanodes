/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_NBD_CLIENT_FAKES_H__
#define __UT_VRT_NBD_CLIENT_FAKES_H__

#include "exaperf/include/exaperf.h"

blockdevice_t *client_get_blockdevice(const exa_uuid_t *uuid)
{
    return NULL;
}

exaperf_t *nbd_clientd_get_exaperf(void)
{
    return NULL;
}

#endif
