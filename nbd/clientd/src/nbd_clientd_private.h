/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef NBD_CLIENTD_PRIVATE_H
#define NBD_CLIENTD_PRIVATE_H

#include "nbd/common/nbd_common.h"
#include "common/include/exa_nodeset.h"

void header_sending(exa_nodeid_t to, const nbd_io_desc_t *io);

#endif /* NBD_CLIENTD_PRIVATE_H */
