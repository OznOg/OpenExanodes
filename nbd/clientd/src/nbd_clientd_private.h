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

#include "common/include/exa_nbd_list.h"

struct client
{
    /* send receive structure */
    struct nbd_list recv_list;
    struct nbd_root_list list_root;
};

typedef struct client client_t;

extern client_t nbd_client;

void header_sending(header_t *header);

#endif /* NBD_CLIENTD_PRIVATE_H */
