/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_MSG_FAKES_H__
#define __UT_VRT_MSG_FAKES_H__

int  vrt_msg_subsystem_init(void)
{
    return 0;
}

void vrt_msg_subsystem_cleanup(void)
{
}
int  vrt_msg_nbd_lock(exa_uuid_t *nbd_uuid, uint64_t start, uint64_t end,
                      int lock)
{
    return 0;
}
int vrt_msg_reintegrate_device(void)
{
    return 0;
}

int vrt_msg_handle;

#endif /* __UT_VRT_MSG_FAKES_H__ */
