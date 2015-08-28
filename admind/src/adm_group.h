/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_GROUP_H
#define __ADM_GROUP_H


#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

#include "admind/services/vrt/sb_version.h"
#include "admind/services/vrt/vrt_layout.h"

/* Data structures */

typedef enum {
  ADM_GROUP_GOAL_UNDEFINED = 0,
  ADM_GROUP_GOAL_STARTED,
  ADM_GROUP_GOAL_STOPPED,
} adm_group_goal_t;

struct adm_volume;
struct adm_disk;

struct adm_group
{
  struct adm_group *next;
  char name[EXA_MAXSIZE_GROUPNAME + 1];

  /* FIXME: this shouldn't be stored there but in the groups
   * superblocks themselves
   */
  vrt_layout_t layout;
  sb_version_t *sb_version;
  exa_uuid_t uuid;
  adm_group_goal_t goal;
  int tainted;
  int committed;
  struct adm_disk *disks;
  struct adm_volume *volumes;
  int nb_volumes;
  int started;
  bool offline;

  /** Tells if the local instance of vrt performed a successful resync on the
   * group. Whenever one node managed to resync a group, no more resync is
   * needed BUT in case of resync failures, the group MUST retry the resync at
   * recovery until it is successful (actually the group is no more OFFLINE */
  bool synched;
};


/* Functions */

struct adm_group *adm_group_alloc(void);
void __adm_group_free(struct adm_group *group);
#define adm_group_free(group) (__adm_group_free(group), group = NULL)

int adm_group_insert_group(struct adm_group *group);
void adm_group_remove_group(struct adm_group *group);
void adm_group_cleanup_group(struct adm_group *group);
struct adm_group *adm_group_get_group_by_uuid(exa_uuid_t *uuid);
struct adm_group *adm_group_get_group_by_name(const char *name);

int adm_group_insert_disk(struct adm_group *group, struct adm_disk *disk);
void adm_group_remove_disk(struct adm_group *group, struct adm_disk *old);
struct adm_disk *adm_group_get_disk_by_uuid(struct adm_group *group,
                                            const exa_uuid_t *uuid);

int adm_group_insert_volume(struct adm_group *group,
			    struct adm_volume *volume);
void adm_group_remove_volume(struct adm_volume *volume);
int adm_group_nb_volumes(struct adm_group *group);
struct adm_volume *adm_group_get_volume_by_name(struct adm_group *group,
						const char *name);
struct adm_volume *adm_group_get_volume_by_uuid(struct adm_group *group,
						const exa_uuid_t *uuid);

/** Get the cumulated size of all the volumes of groups
 *
 * @return the size of the volumes in KB
 */
uint64_t adm_group_get_total_size(void);

struct adm_group *adm_group_get_next(struct adm_group *group);
struct adm_group *adm_group_get_first(void);

#define adm_group_for_each_group(group) \
  for((group) = adm_group_get_first(); \
      (group); \
      (group) = adm_group_get_next(group) \
  )

#define adm_group_for_each_volume(group, volume) \
  for(volume = group->volumes; \
      volume; \
      volume = volume->next \
  )

#define adm_group_for_each_disk(group, disk) \
  for(disk = group->disks; \
      disk; \
      disk = disk->next_in_group \
  )

#endif /* __ADM_GROUP_H */
