/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _NBD_CLIENTD_NBD_CLIENTD_H
#define _NBD_CLIENTD_NBD_CLIENTD_H

#include "blockdevice/include/blockdevice.h"
#include "common/include/uuid.h"

#define DEFAULT_MAX_CLIENT_REQUESTS 75

blockdevice_t *client_get_blockdevice(const exa_uuid_t *uuid);

#endif /* _NBD_CLIENTD_NBD_CLIENTD_H */
