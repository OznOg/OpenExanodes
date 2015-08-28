/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TM_TOKEN_H
#define TM_TOKEN_H

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

#include "os/include/os_network.h"

/* XXX Ideally, the holder should be a node UUID instead of a node id */
typedef struct
{
    exa_uuid_t uuid;        /**< Token UUID */
    exa_nodeid_t holder;    /**< Node holding the token */
    os_net_addr_str_t holder_addr;  /**< IP address of the holder */
} token_t;

#endif /* TM_TOKEN_H */
