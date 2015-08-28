/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_MODULE_H__
#define __VRT_MODULE_H__


#include "common/include/uuid.h"

#include "blockdevice/include/blockdevice.h"

#include "vrt/common/include/list.h"
#include "vrt/virtualiseur/include/vrt_group.h"

int  vrt_groups_list_add(struct vrt_group *group);
void vrt_groups_list_del(struct vrt_group *group);

struct vrt_group *vrt_get_group_from_uuid(const exa_uuid_t *uuid);
struct vrt_group *vrt_get_group_from_name(const char *group_name);

/**
 * Get a volume from its uuid.
 *
 * @param[in] uuid  uuid of the volume to get.
 *
 * @return volume corresponding to uuid of NULL if not found
 *         (not found means 'does not exist' OR 'group that holds the volume
 *          is not started')
 */
struct vrt_volume *vrt_get_volume_from_uuid(const exa_uuid_t *uuid);

blockdevice_t *vrt_open_volume(const exa_uuid_t *vol_uuid,
                               blockdevice_access_t access);
int vrt_close_volume(blockdevice_t *volume_handle);

exa_bool_t vrt_barriers_enabled(void);

#endif /* __VRT_MODULE_H__ */
