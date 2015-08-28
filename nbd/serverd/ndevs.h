/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef _NBD_SERVERD_NDEVS_H
#define _NBD_SERVERD_NDEVS_H

#include "nbd/serverd/nbd_serverd.h"
#include "nbd/serverd/nbd_disk_thread.h"

extern int export_device(const exa_uuid_t *uuid, char *device_path);

extern int unexport_device(const exa_uuid_t *uuid);

extern int server_add_client (char *node_name, char *net_id,
			      exa_nodeid_t node_id);

extern int server_remove_client(exa_nodeid_t node_id);

extern void nbd_ndev_getinfo(const exa_uuid_t *uuid, ExamsgID from);

extern void rebuild_helper_thread(void *p);

extern void nbd_rdev_check(int major, int minor, Examsg *msg, ExamsgMID *from);

extern int get_client_id_from_node_id(exa_nodeid_t node_id);

#endif /* _NBD_SERVERD_NDEVS_H */
