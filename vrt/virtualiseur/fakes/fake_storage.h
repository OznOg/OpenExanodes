/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef FAKE_STORAGE_H
#define FAKE_STORAGE_H

#include "vrt/virtualiseur/include/storage.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"

#include "os/include/os_inttypes.h"

/* Creates one spof per rdev */
storage_t *make_fake_storage(uint32_t num_spof_groups, uint32_t chunk_size,
                             struct vrt_realdev *rdevs[], unsigned num_rdevs);

#endif /* FAKE_STORAGE_H */
