/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_group.h"

#include <string.h>
#include <errno.h>

#include "os/include/os_dir.h"
#include "os/include/os_stdio.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_volume.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"

struct adm_group *_groups = NULL;
int groups_number = 0;

struct adm_group *adm_group_get_first(void)
{
    return _groups;
}

struct adm_group *adm_group_get_next(struct adm_group *group)
{
    EXA_ASSERT(group != NULL);

    return group->next;
}

/* group */

struct adm_group *
adm_group_alloc(void)
{
  struct adm_group *group;

  group = os_malloc(sizeof(struct adm_group));
  if (group == NULL)
    return NULL;

  memset(group, 0, sizeof(struct adm_group));

  return group;
}


void
__adm_group_free(struct adm_group *group)
{
  EXA_ASSERT(group->next == NULL);

  os_free(group);
}


/* disks */

int
adm_group_insert_disk(struct adm_group *group, struct adm_disk *new)
{
  struct adm_disk *curr;
  struct adm_disk *prev;

  EXA_ASSERT(group != NULL);
  EXA_ASSERT(new != NULL);
  EXA_ASSERT(uuid_is_zero(&new->group_uuid));
  EXA_ASSERT(new->next_in_group == NULL);
  EXA_ASSERT(!uuid_is_zero(&new->vrt_uuid));

  curr = group->disks;
  prev = NULL;

  /* For an obscure reason explained in bug #1808, disks MUST be sorted by
   * UUID as the VRT must receive always the disks in the same order. */
  while (curr && uuid_compare(&new->vrt_uuid, &curr->vrt_uuid) < 0)
  {
    prev = curr;
    curr = curr->next_in_group;
  }

  /* check if the disk was already in the group */
  if (curr && uuid_compare(&new->vrt_uuid, &curr->vrt_uuid) == 0)
    return -EEXIST;

  uuid_copy(&new->group_uuid, &group->uuid);

  if (prev == NULL)
    group->disks = new;
  else
    prev->next_in_group = new;

  if (curr != NULL)
    new->next_in_group = curr;

  return EXA_SUCCESS;
}


void adm_group_remove_disk(struct adm_group *group, struct adm_disk *old)
{
  struct adm_disk *disk;
  struct adm_disk *prev;

  EXA_ASSERT(old != NULL);
  EXA_ASSERT(!uuid_is_zero(&old->group_uuid));
  EXA_ASSERT(uuid_is_equal(&old->group_uuid, &group->uuid));

  disk = group->disks;
  prev = NULL;

  while (disk && disk != old)
  {
    prev = disk;
    disk = disk->next_in_group;
  }

  EXA_ASSERT(disk == old);

  if (prev == NULL)
    group->disks = old->next_in_group;
  else
    prev->next_in_group = old->next_in_group;

  uuid_zero(&old->vrt_uuid);
  old->next_in_group = NULL;
  uuid_zero(&old->group_uuid);
}


struct adm_disk *
adm_group_get_disk_by_uuid(struct adm_group *group, const exa_uuid_t *uuid)
{
  struct adm_disk *disk;

  EXA_ASSERT(group != NULL);
  EXA_ASSERT(uuid != NULL);

  adm_group_for_each_disk(group, disk)
  {
    if (uuid_is_equal(&disk->uuid, uuid))
      return disk;
  }

  return NULL;
}

/* volumes */

int
adm_group_insert_volume(struct adm_group *group, struct adm_volume *new)
{
  struct adm_volume *curr;
  struct adm_volume *prev;

  EXA_ASSERT(group != NULL);
  EXA_ASSERT(new != NULL);
  EXA_ASSERT(new->group == NULL);
  EXA_ASSERT(new->next == NULL);

  if (adm_group_get_volume_by_name(group, new->name) != NULL)
    return -ADMIND_ERR_VOLUME_ALREADY_CREATED;

  curr = group->volumes;
  prev = NULL;

  while (curr && uuid_compare(&new->uuid, &curr->uuid) < 0)
  {
    prev = curr;
    curr = curr->next;
  }

  if (curr && uuid_compare(&new->uuid, &curr->uuid) == 0)
    return -ADMIND_ERR_VOLUME_ALREADY_CREATED;

  new->group = group;

  if (prev == NULL)
    group->volumes = new;
  else
    prev->next = new;

  if (curr != NULL)
    new->next = curr;

  group->nb_volumes++;

  return EXA_SUCCESS;
}


void
adm_group_remove_volume(struct adm_volume *old)
{
  struct adm_volume *volume;
  struct adm_volume *prev;

  EXA_ASSERT(old != NULL);
  EXA_ASSERT(old->group != NULL);

  volume = old->group->volumes;
  prev = NULL;

  while (volume && volume != old)
  {
    prev = volume;
    volume = volume->next;
  }

  EXA_ASSERT(volume == old);

  old->group->nb_volumes--;

  if (prev == NULL)
    old->group->volumes = old->next;
  else
    prev->next = old->next;

  old->next = NULL;
  old->group = NULL;
}


int
adm_group_nb_volumes(struct adm_group *group)
{
  return group->nb_volumes;
}


struct adm_volume *
adm_group_get_volume_by_name(struct adm_group *group, const char *name)
{
  struct adm_volume *volume;

  EXA_ASSERT(group != NULL);
  EXA_ASSERT(name != NULL);

  adm_group_for_each_volume(group, volume)
  {
    if (!strncmp(volume->name, name, EXA_MAXSIZE_VOLUMENAME + 1))
      return volume;
  }

  return NULL;
}


struct adm_volume *
adm_group_get_volume_by_uuid(struct adm_group *group,
			     const exa_uuid_t *uuid)
{
  struct adm_volume *volume;

  EXA_ASSERT(group != NULL);
  EXA_ASSERT(uuid != NULL);

  adm_group_for_each_volume(group, volume)
  {
    if (uuid_is_equal(&volume->uuid, uuid))
      return volume;
  }

  return NULL;
}

int
adm_group_insert_group(struct adm_group *new)
{
  struct adm_group *curr;
  struct adm_group *prev;

  EXA_ASSERT(new != NULL);
  EXA_ASSERT(new->next == NULL);

  if (adm_group_get_group_by_name(new->name) != NULL)
    return -ADMIND_ERR_GROUP_ALREADY_CREATED;

  curr = _groups;
  prev = NULL;

  while (curr && uuid_compare(&new->uuid, &curr->uuid) < 0)
  {
    prev = curr;
    curr = curr->next;
  }

  if (curr && uuid_compare(&new->uuid, &curr->uuid) == 0)
    return -EEXIST;

  if (prev == NULL)
    _groups = new;
  else
    prev->next = new;

  if (curr != NULL)
    new->next = curr;

  groups_number++;

  return EXA_SUCCESS;
}


void
adm_group_remove_group(struct adm_group *old)
{
  struct adm_group *group;
  struct adm_group *prev;

  EXA_ASSERT(old != NULL);

  group = _groups;
  prev = NULL;

  while (group && group != old)
  {
    prev = group;
    group = group->next;
  }

  EXA_ASSERT(group == old);

  if (prev == NULL)
    _groups = old->next;
  else
    prev->next = old->next;

  old->next = NULL;

  groups_number--;
}


void
adm_group_cleanup_group(struct adm_group *group)
{

    while (group->volumes)
    {
	struct adm_volume *volume = group->volumes;

	adm_group_remove_volume(volume);
	adm_volume_free(volume);
    }

    while (group->disks)
    {
	struct adm_disk *disk = group->disks;
	/* Careful !!! Here the disk is NOT deleted because the group does not
	 * have the ownership of the disk. */
	adm_group_remove_disk(group, disk);
    }
}


struct adm_group *
adm_group_get_group_by_uuid(exa_uuid_t *uuid)
{
  struct adm_group *group;

  EXA_ASSERT(uuid != NULL);

  adm_group_for_each_group(group)
  {
    if (uuid_is_equal(&group->uuid, uuid))
      return group;
  }

  return NULL;
}


struct adm_group *
adm_group_get_group_by_name(const char *name)
{
  struct adm_group *group;

  EXA_ASSERT(name != NULL);

  adm_group_for_each_group(group)
  {
    if (strncmp(group->name, name, EXA_MAXSIZE_GROUPNAME + 1) == 0)
      return group;
  }

  return NULL;
}


uint64_t adm_group_get_total_size(void)
{
    uint64_t total_size = 0;
    struct adm_group *group;
    const struct adm_volume *volume;

    adm_group_for_each_group(group)
        adm_group_for_each_volume(group, volume)
            total_size += volume->size;

    return total_size;
}
