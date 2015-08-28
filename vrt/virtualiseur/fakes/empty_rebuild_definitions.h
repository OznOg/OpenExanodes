/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EMPTY_REBUILD_DEFINITIONS_H
#define EMPTY_REBUILD_DEFINITIONS_H

int vrt_group_rebuild_thread_start(struct vrt_group *group)
{
    return 0;
}

void vrt_group_rebuild_thread_cleanup(struct vrt_group *group)
{
}

void vrt_group_rebuild_thread_wakeup(struct vrt_group *group)
{
}

#endif /* EMPTY_REBUILD_DEFINITIONS_H */
