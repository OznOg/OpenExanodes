/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef BD_USER_USER_H
#define BD_USER_USER_H

#include "nbd/clientd/src/nbd_stats.h"
#include "nbd/common/nbd_common.h"
#include "blockdevice/include/blockdevice.h"

#include "common/include/uuid.h"
#include "common/include/exa_nodeset.h"

#include "os/include/os_inttypes.h"

#define EXA_BDEV_NAME_MAXLEN 127

typedef struct __ndev ndev_t;

bool exa_bdinit(int buffer_size, int max_queue, bool barrier_enable);
void exa_bdend(void);

void exa_bd_end_request(header_t *header);

void *exa_bdget_buffer(int num);

void bd_get_stats(struct nbd_stats_reply *reply, const exa_uuid_t *uuid, bool reset);

const char *ndev_get_name(const ndev_t *ndev);

blockdevice_t *exa_bdget_block_device(const exa_uuid_t *uuid);

int client_import_device(const exa_uuid_t *uuid, exa_nodeid_t node_id,
                         uint64_t size_in_sector, int device_nb);
int client_remove_device(const exa_uuid_t *uuid);

int client_suspend_device(const exa_uuid_t *uuid);

int client_down_device(const exa_uuid_t *uuid);

int client_resume_device(const exa_uuid_t *uuid);

/* FIXME for some reason, there is no client_up_device() which seems
 * quite strange... I guess the up is performed as a side effect somewhere... */

#endif  /* BD_USER_USER_H */
