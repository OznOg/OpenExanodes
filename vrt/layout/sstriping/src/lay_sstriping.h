/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_LAYOUT_SSTRIPING_H__
#define __VRT_LAYOUT_SSTRIPING_H__

#include "common/include/uuid.h"

#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_layout.h"

#include "vrt/layout/sstriping/src/lay_sstriping_superblock.h"

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/assembly/src/assembly_volume.h"

/* striping */

void sstriping_volume2rdev(struct vrt_volume *volume, uint64_t vsector,
			   struct vrt_realdev **rdev, uint64_t *rsector);

/* request */

vrt_req_status_t sstriping_build_io_for_req(struct vrt_request *vrt_req);
void sstriping_init_req (struct vrt_request *vrt_req);
void sstriping_cancel_req (struct vrt_request *vrt_req);
void sstriping_declare_io_needs(struct vrt_request *vrt_req,
                                unsigned int *io_count,
                                bool *sync_afterward);
int sstriping_max_sectors(struct vrt_volume *volume);

#endif /* __VRT_LAYOUT_SSTRIPING_H__ */
