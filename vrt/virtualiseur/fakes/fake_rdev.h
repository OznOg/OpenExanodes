/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef FAKE_RDEV_H
#define FAKE_RDEV_H

#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/common/include/spof.h"

#include "blockdevice/include/blockdevice.h"

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"

struct vrt_realdev *make_fake_rdev(exa_nodeid_t node_id, spof_id_t spof_id,
                                   const exa_uuid_t *uuid,
                                   const exa_uuid_t *nbd_uuid,
                                   uint64_t real_size,
                                   int local, bool up);

blockdevice_t *make_fake_blockdevice(uint64_t sector_count);

#endif /* FAKE_RDEV_H */
