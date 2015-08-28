/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_CLIENT_H__
#define __VRT_CLIENT_H__

#include "vrt/virtualiseur/include/vrt_msg.h"

/* FIXME Reorder functions. And make their implementation in vrt_client.c
         match the order hereafter! */

/*** Recovery ***/

int vrt_client_nodes_status       (ExamsgHandle h, exa_nodeset_t *nodes_up);
int vrt_client_device_up          (ExamsgHandle h, const exa_uuid_t * group_uuid,
				   const exa_uuid_t *rdev_uuid);
int vrt_client_device_down        (ExamsgHandle h, const exa_uuid_t * group_uuid,
				   const exa_uuid_t *rdev_uuid);

int vrt_client_device_replace(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                              const exa_uuid_t *vrt_uuid,
                              const exa_uuid_t *rdev_uuid);

int vrt_client_device_add(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                          const exa_uuid_t *vrt_uuid,
                          const exa_uuid_t *rdev_uuid);

int vrt_client_device_reset(ExamsgHandle h, const exa_uuid_t *group_uuid,
                            const exa_uuid_t *vrt_uuid);

int vrt_client_device_reintegrate(ExamsgHandle h, const exa_uuid_t * group_uuid,
				  const exa_uuid_t *rdev_uuid);
int vrt_client_device_post_reintegrate(ExamsgHandle h, const exa_uuid_t * group_uuid,
				       const exa_uuid_t *rdev_uuid);

int vrt_client_group_suspend      (ExamsgHandle h, const exa_uuid_t *group_uuid);
int vrt_client_group_resume       (ExamsgHandle h, const exa_uuid_t *group_uuid);
int vrt_client_group_resync(ExamsgHandle h, const exa_uuid_t *group_uuid,
                            const exa_nodeset_t *nodes);
int vrt_client_group_post_resync  (ExamsgHandle h, const exa_uuid_t *group_uuid);
int vrt_client_group_suspend_metadata_and_rebuild(ExamsgHandle h,
                                                  const exa_uuid_t *group_uuid);
int vrt_client_group_resume_metadata_and_rebuild(ExamsgHandle h,
                                                 const exa_uuid_t *group_uuid);

int vrt_client_group_compute_status(ExamsgHandle h, const exa_uuid_t *group_uuid);
int vrt_client_group_wait_initialized_requests (ExamsgHandle h, const exa_uuid_t *group_uuid);
int vrt_client_get_volume_status  (ExamsgHandle h, const exa_uuid_t *group_uuid, const exa_uuid_t *volume_uuid);

/*** Commands ***/
int vrt_client_group_add_rdev (ExamsgHandle mh, const exa_uuid_t *group_uuid,
                               exa_nodeid_t node_id, spof_id_t spof_id,
                               exa_uuid_t *uuid, exa_uuid_t *nbd_uuid,
                               int local, bool up);
int vrt_client_group_start    (ExamsgHandle mh, const exa_uuid_t *group_uuid);
int vrt_client_group_create   (ExamsgHandle mh, const exa_uuid_t *group_uuid,
                               uint32_t slot_width, uint32_t chunk_size, uint32_t su_size,
                               uint32_t dirty_zone_size, uint32_t blended_stripes, uint32_t nb_spare,
                               char *error_msg);
int vrt_client_group_stop     (ExamsgHandle mh, const exa_uuid_t *group_uuid);
int vrt_client_group_insert_rdev(ExamsgHandle mh, const exa_uuid_t *group_uuid,
                                 exa_nodeid_t node_id, spof_id_t spof_id,
                                 exa_uuid_t *uuid, exa_uuid_t *nbd_uuid,
                                 int local, uint64_t old_sb_version,
                                 uint64_t new_sb_version);
int vrt_client_group_stoppable (ExamsgHandle mh, const exa_uuid_t *group_uuid);
int vrt_client_group_going_offline (ExamsgHandle mh, const exa_uuid_t *group_uuid, const exa_nodeset_t *stop_nodes);
int vrt_client_group_begin    (ExamsgHandle mh, const char *group_name,
                               const exa_uuid_t *group_uuid,
                               const char *layout_name,
                               uint64_t sb_version);
int vrt_client_group_sync_sb  (ExamsgHandle mh, const exa_uuid_t *group_uuid,
                               uint64_t old_sb_version, uint64_t new_sb_version);
int vrt_client_group_freeze   (ExamsgHandle mh, const exa_uuid_t *group_uuid);
int vrt_client_group_unfreeze (ExamsgHandle mh, const exa_uuid_t *group_uuid);
int vrt_client_group_reset(ExamsgHandle mh, const exa_uuid_t *group_uuid);
int vrt_client_group_check(ExamsgHandle mh, const exa_uuid_t *group_uuid);

int vrt_client_volume_create (ExamsgHandle mh, const exa_uuid_t *group_uuid,
                              const char *volume_name, const exa_uuid_t *volume_uuid, uint64_t size);
int vrt_client_volume_start(ExamsgHandle mh, const exa_uuid_t *group_uuid, const exa_uuid_t *volume_uuid);
int vrt_client_volume_stop (ExamsgHandle mh, const exa_uuid_t *group_uuid, const exa_uuid_t *volume_uuid);
int vrt_client_volume_delete (ExamsgHandle mh, const exa_uuid_t *group_uuid, const exa_uuid_t *volume_uuid);
int vrt_client_volume_resize (ExamsgHandle mh, const exa_uuid_t *group_uuid, const exa_uuid_t *volume_uuid,
			      uint64_t size);

/*** Info ***/

int vrt_client_group_info  (ExamsgHandle mh, const exa_uuid_t *groupUuid,
			    struct vrt_group_info *group_info);
int vrt_client_volume_info (ExamsgHandle mh, const exa_uuid_t *groupUuid,
			    const exa_uuid_t *volumeUuid,
			    struct vrt_volume_info *volume_info);
int vrt_client_rdev_info   (ExamsgHandle mh, const exa_uuid_t *groupUuid,
			    const exa_uuid_t *rdev_uuid,
			    struct vrt_realdev_info *realdev_info);
int vrt_client_rdev_rebuild_info(ExamsgHandle mh, const exa_uuid_t *groupUuid,
                                 const exa_uuid_t *rdev_uuid,
                                 struct vrt_realdev_rebuild_info *rebuild_info);
int vrt_client_rdev_reintegrate_info(ExamsgHandle mh, const exa_uuid_t *groupUuid,
                                     const exa_uuid_t  *rdev_uuid,
                                     struct vrt_realdev_reintegrate_info *realdev_info);

/*** Statistics ***/

int vrt_client_stat_get(ExamsgHandle mh, struct vrt_stats_request *request,
			struct vrt_stats_reply *stats);

/*** Misc ***/

int vrt_client_pending_group_cleanup(ExamsgHandle mh);

/* --- Utility functions (util_vrt.c) --------------------------------- */
const char* vrtd_group_status_str(exa_group_status_t status);
const char* vrtd_realdev_status_str(exa_realdev_status_t status);

#endif
