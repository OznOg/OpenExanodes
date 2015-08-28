/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_VOLUME_H
#define __ADM_VOLUME_H

#include "admind/src/adm_group.h"
#ifdef WITH_FS
#include "admind/src/adm_fs.h"
#endif

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "vrt/virtualiseur/include/vrt_common.h"

#include <stdlib.h>

struct adm_volume
{
    struct adm_volume *next;
    struct adm_group *group;
#ifdef WITH_FS
    struct adm_fs* filesystem;
#endif
    char name[EXA_MAXSIZE_VOLUMENAME + 1];
    exa_uuid_t uuid;
    uint64_t size;                          /**< size of the volume in KB */
    int shared;
    exa_nodeset_t goal_stopped;
    exa_nodeset_t goal_started;
    exa_nodeset_t goal_readonly;
    int committed;
    int started;
    bool exported;
    int readonly;
    uint32_t readahead; /* Readahead value in KB. Not used with iSCSI. */
};

struct adm_volume *adm_volume_alloc(void);
void __adm_volume_free(struct adm_volume *volume);
#define adm_volume_free(volume) (__adm_volume_free(volume), volume = NULL)

void adm_volume_set_goal(struct adm_volume *volume, exa_nodeset_t *hostlist,
			 exa_volume_status_t status, int readonly);

int adm_volume_is_in_fs(struct adm_volume *volume);

void adm_volume_path(char *path, size_t len, const char *group_name,
		     const char *volume_name);

bool adm_volume_is_started(const exa_uuid_t *uuid, const exa_nodeid_t nodeid);

bool adm_volume_is_goal_readonly(const exa_uuid_t *uuid, const exa_nodeid_t nodeid);

void adm_volume_set_exported(const exa_uuid_t *uuid, bool exported);

bool adm_volume_is_exported(const exa_uuid_t *uuid);

bool adm_volume_is_offline(const exa_uuid_t *uuid);

uint32_t adm_volume_get_readahead(const exa_uuid_t *uuid);

#endif /* __ADM_VOLUME_H */
