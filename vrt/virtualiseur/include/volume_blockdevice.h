/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef BDEV_BLOCKDEVICE_H
#define BDEV_BLOCKDEVICE_H

#include "blockdevice/include/blockdevice.h"
#include "vrt/virtualiseur/include/vrt_volume.h"

int volume_blockdevice_open(blockdevice_t **blockdevice, vrt_volume_t *volume,
                          blockdevice_access_t access);

#endif /* BDEV_BLOCKDEVICE_H */
