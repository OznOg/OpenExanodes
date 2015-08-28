/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_CMD_FAKES_H__
#define __UT_VRT_CMD_FAKES_H__

int vrt_cmd_threads_init(void)
{
    return 0;
}

void vrt_cmd_threads_cleanup(void)
{
}

void *vrt_cmd_thread_queue_get(int id)
{
    return NULL;
}

#endif /* __UT_VRT_CMD_FAKES_H__ */
