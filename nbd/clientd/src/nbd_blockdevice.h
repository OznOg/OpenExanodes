/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef NBD_BLOCKDEVICE_H
#define NBD_BLOCKDEVICE_H

#include "blockdevice/include/blockdevice.h"
#include "nbd/clientd/src/bd_user_user.h"

typedef void nbd_make_request_t(ndev_t *ndev, blockdevice_io_t *bio);

int nbd_blockdevice_open(blockdevice_t **blockdevice,
                         blockdevice_access_t access,
                         int max_io_size,
                         nbd_make_request_t *make_request,
                         ndev_t *ndev);

#endif /* NBD_BLOCKDEVICE_H */
