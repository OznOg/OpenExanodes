/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SERVICE_VRT_H__
#define __SERVICE_VRT_H__

#include "admind/src/admind.h"
#include "examsg/include/examsg.h"
#include "lum/export/include/export.h"

struct adm_group;
struct adm_disk;
struct adm_volume;

int service_vrt_prepare_group(struct adm_group *group);
int local_exa_dgstart_vrt_start(struct adm_group *group);

/* volume commands */
int vrt_master_volume_stop(int thr_nb, struct adm_volume *volume,
                           const exa_nodeset_t *nodelist, bool force,
                           adm_goal_change_t goal_change, bool print_warning);

int vrt_master_volume_start(int thr_nb, struct adm_volume *volume,
			    const exa_nodeset_t *nodelist, uint32_t readonly,
                            bool print_warning);

int vrt_master_volume_start_all(int thr_nb, struct adm_group *group);

int vrt_master_volume_delete_all(int thr_nb, struct adm_group *group,
				 bool metadata_recovery);

int vrt_master_volume_delete(int thr_nb, struct adm_volume *volume,
			     bool metadata_recovery);

int vrt_master_volume_create(int thr_nb, struct adm_group *, const char* volume_name,
			     export_type_t export_type, uint64_t sizeKB,
			     int isprivate, uint32_t readahead);

int vrt_master_volume_resize(int thr_nb, struct adm_volume *volume,
			     uint64_t sizeKB);

int vrt_master_volume_tune_readahead(int thr_nb, const struct adm_volume *volume,
				     uint32_t read_ahead);

int adm_vrt_group_sync_sb(int thr_nb, struct adm_group *group);

int service_vrt_group_stop(struct adm_group *group, bool force);

/**
 * Synchronize sb_version metadata for a group.
 *
 * @param[in] thr_nb   Thread number
 * @param[in] group    Pointer to the group
 *
 * @return EXA_SUCCESS or -ADMIND_ERR_NODE_DOWN in case of a node failure
 */
int vrt_group_sync_sb_versions(int thr_nb, struct adm_group *group);

/* Volume manipulations helpers, for commands that need vrt rebuild and metadata
 * threads suspending/resuming with barriers.
 */

/**
 * Suspend the VRT rebuild and metadata flush threads for the given groupon all
 * nodes, and waits on a barrier for all nodes to be done.
 *
 * @param[in]   thr_nb      The thread number
 * @param[in]   group_uuid  The UUID of the group
 *
 * @return  The barrier return value, which will be 0 if all nodes succeeded,
 *          or -ADMIND_ERR_NODE_DOWN if a node failed during the command.
 *          The thread suspending itself never fails, so no other error code
 *          will be returned.
 */
int vrt_group_suspend_threads_barrier(int thr_nb, const exa_uuid_t *group_uuid);

/**
 * Resume the VRT rebuild and metadata flush threads for the given groupon all
 * nodes, and waits on a barrier for all nodes to be done.
 *
 * @param[in]   thr_nb      The thread number
 * @param[in]   group_uuid  The UUID of the group
 *
 * @return  The barrier return value, which will be 0 if all nodes succeeded,
 *          or -ADMIND_ERR_NODE_DOWN if a node failed during the command.
 *          The thread resuming itself never fails, so no other error code
 *          will be returned.
 */
int vrt_group_resume_threads_barrier(int thr_nb, const exa_uuid_t *group_uuid);


#endif /* __SERVICE_VRT_H__ */
