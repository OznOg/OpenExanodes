

/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_DISK_H
#define __ADM_DISK_H


#include "common/include/uuid.h"
#include "common/include/exa_constants.h"

#include "common/include/exa_nodeset.h"

#include "rdev/include/exa_rdev.h"

/* FIXME 'state' field should be an enum */
struct adm_disk_local
{
  exa_rdev_handle_t *rdev_req;
  int reachable;
  int state;  /* EXA_RDEV_STATUS_OK or EXA_RDEV_STATUS_FAIL */
};

struct adm_disk
{
  struct adm_disk *next_in_group;
  struct adm_disk *next_in_node;
  char path[EXA_MAXSIZE_DEVPATH + 1];
  exa_uuid_t uuid;     /* UUID used inside RDEV and NBD */
  exa_uuid_t vrt_uuid; /* UUID used inside VRT and to sort disks in group in admind. */
  exa_nodeid_t node_id;
  exa_uuid_t group_uuid; /* zeroed when the disk isn't in a group */

  int broken;
  int imported;
  int suspended;
  int up_in_vrt;

  struct adm_disk_local *local;
};


/* Functions */

struct adm_disk *adm_disk_alloc(void);
int adm_disk_local_new(struct adm_disk *disk);
void adm_disk_delete(struct adm_disk *disk);
int adm_disk_is_local(struct adm_disk *disk);
const char *adm_disk_get_status_str(struct adm_disk *disk);


#endif /* __ADM_DISK_H */
