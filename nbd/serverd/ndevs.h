/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef _NBD_SERVERD_NDEVS_H
#define _NBD_SERVERD_NDEVS_H

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

#include "examsg/include/examsg.h"

int export_device(const exa_uuid_t *uuid, char *device_path);

int unexport_device(const exa_uuid_t *uuid);

int server_add_client (char *node_name, char *net_id, exa_nodeid_t node_id);

int server_remove_client(exa_nodeid_t node_id);

void nbd_ndev_getinfo(const exa_uuid_t *uuid, ExamsgID from);

void rebuild_helper_thread(void *p);

#endif
