/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_disk.h"

#include <errno.h>
#include <string.h>

#include "common/include/exa_error.h"
#include "os/include/os_mem.h"
#include "log/include/log.h"


struct adm_disk *
adm_disk_alloc(void)
{
  struct adm_disk *disk;

  disk = os_malloc(sizeof(struct adm_disk));
  if (disk == NULL)
    return NULL;

  memset(disk, 0, sizeof(struct adm_disk));

  disk->node_id = EXA_NODEID_NONE;
  uuid_zero(&disk->group_uuid);

  return disk;
}


int
adm_disk_local_new(struct adm_disk *disk)
{
  disk->local = os_malloc(sizeof(struct adm_disk_local));
  if (disk->local == NULL)
  {
    exalog_error("Failed to alloc memory");
    return -ENOMEM;
  }

  memset(disk->local, 0, sizeof(struct adm_disk_local));

  return EXA_SUCCESS;
}


void
adm_disk_delete(struct adm_disk *disk)
{
  EXA_ASSERT(disk);
  EXA_ASSERT(disk->node_id == EXA_NODEID_NONE);
  EXA_ASSERT(disk->next_in_node == NULL);
  EXA_ASSERT(disk->next_in_group == NULL);

  if (disk->local != NULL)
    os_free(disk->local);

  os_free(disk);
}


int
adm_disk_is_local(struct adm_disk *disk)
{
  return disk->local != NULL;
}

/**
 * Get the status string for a disk.
 * @param [in] disk     The disk for which we want the status
 *
 * @return One of ADMIND_PROP_BROKEN, ADMIND_PROP_DOWN,
 *                ADMIND_PROP_UP or, for local disks only,
 *                ADMIND_PROP_MISSING.
 *
 * FIXME: We should have a way to know if a non-local disk is
 * missing.
 * FIXME: This should be an enum.
 * FIXME: This is only for unassigned disks. Disks in groups
 * have a state defined in the VRT (see vrtd_realdev_status_str)
 */
const char *adm_disk_get_status_str(struct adm_disk *disk)
{
    if (disk->broken)
        return ADMIND_PROP_BROKEN;
    else if (adm_disk_is_local(disk) && !disk->local->reachable)
        return ADMIND_PROP_MISSING;
    else if (!disk->imported)
        return ADMIND_PROP_DOWN;
    else
        return ADMIND_PROP_UP;
}
