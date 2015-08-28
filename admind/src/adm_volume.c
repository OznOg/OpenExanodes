/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_volume.h"
#include "admind/src/adm_group.h"

#include <string.h>

#ifdef WITH_FS
#include "admind/src/adm_fs.h"
#endif

#include "common/include/exa_assert.h"
#include "common/include/exa_names.h"
#include "os/include/os_mem.h"

#include "os/include/os_stdio.h"

struct adm_volume *
adm_volume_alloc(void)
{
  struct adm_volume *volume;

  volume = os_malloc(sizeof(struct adm_volume));
  if (volume == NULL)
    return NULL;

  memset(volume, 0, sizeof(struct adm_volume));
  volume->group = NULL;

  return volume;
}


void
__adm_volume_free(struct adm_volume *volume)
{
  EXA_ASSERT(volume);
  EXA_ASSERT(volume->group == NULL);
  EXA_ASSERT(volume->next == NULL);
#ifdef WITH_FS
  if (volume->filesystem)
    {
      adm_fs_free(volume->filesystem);
      volume->filesystem = NULL;
    }
#endif
  os_free(volume);
}


void
adm_volume_set_goal(struct adm_volume *volume, exa_nodeset_t *hostlist,
		    exa_volume_status_t status, int readonly)
{
  exa_nodeid_t node;

  EXA_ASSERT(volume != NULL);

  /* Iterate through the hostlist in order to remove every node from
   * all the status lists and finally add it to the right one
   * according to its new status.
   */

  exa_nodeset_foreach(hostlist, node)
  {
    exa_nodeset_del(&volume->goal_stopped, node);
    exa_nodeset_del(&volume->goal_started, node);
    exa_nodeset_del(&volume->goal_readonly, node);

    switch (status)
    {
    case EXA_VOLUME_STOPPED:
      exa_nodeset_add(&volume->goal_stopped, node);
      break;

    case EXA_VOLUME_STARTED:
      exa_nodeset_add(&volume->goal_started, node);
      break;
    }

    if (readonly)
      exa_nodeset_add(&volume->goal_readonly, node);
  }
}


int
adm_volume_is_in_fs(struct adm_volume *volume)
{
#ifdef WITH_FS
    if (!volume)
    {
      return false;
    }
  return (volume->filesystem != NULL);
#else
  return false;
#endif
}


void
adm_volume_path(char *path, size_t len, const char *group_name,
		const char *volume_name)
{
  os_snprintf(path, len, "/dev/" DEV_ROOT_NAME "/%s/%s", group_name,
	   volume_name);
}

static struct adm_volume *adm_volume_get_by_uuid(const exa_uuid_t *uuid)
{
    struct adm_group *group;
    struct adm_volume *volume;

    if (uuid == NULL)
        return NULL;

    adm_group_for_each_group(group)
    {
        adm_group_for_each_volume(group, volume)
        {
            if (uuid_is_equal(&volume->uuid, uuid))
                return volume;
        }
    }
    return NULL;
}

bool adm_volume_is_started(const exa_uuid_t *uuid, const exa_nodeid_t nodeid)
{
    struct adm_volume *volume = adm_volume_get_by_uuid(uuid);
    EXA_ASSERT(volume != NULL);

    /* FIXME should we check the goal_started? */
    return volume->started;
}

bool adm_volume_is_offline(const exa_uuid_t *uuid)
{
    struct adm_volume *volume = adm_volume_get_by_uuid(uuid);
    EXA_ASSERT(volume != NULL);

    return volume->group->offline;
}

bool adm_volume_is_goal_readonly(const exa_uuid_t *uuid,
                                 const exa_nodeid_t nodeid)
{
    struct adm_volume *volume = adm_volume_get_by_uuid(uuid);
    EXA_ASSERT(volume != NULL);

    return exa_nodeset_contains(&volume->goal_readonly, nodeid);
}

void adm_volume_set_exported(const exa_uuid_t *uuid, bool exported)
{
    struct adm_volume *volume = adm_volume_get_by_uuid(uuid);
    EXA_ASSERT(volume != NULL);

    volume->exported = exported;
}

bool adm_volume_is_exported(const exa_uuid_t *uuid)
{
    struct adm_volume *volume = adm_volume_get_by_uuid(uuid);
    EXA_ASSERT(volume != NULL);

    return volume->exported;
}

uint32_t adm_volume_get_readahead(const exa_uuid_t *uuid)
{
    struct adm_volume *volume = adm_volume_get_by_uuid(uuid);
    EXA_ASSERT(volume != NULL);

    return volume->readahead;
}
