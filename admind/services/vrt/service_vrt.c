/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/include/service_vrt.h"
#include "admind/include/service_lum.h"

#include <errno.h>
#include <string.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/admindstate.h"
#include "admind/src/deviceblocks.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/service_parameter.h"
#include "lum/client/include/lum_client.h"
#include "common/include/exa_names.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "os/include/os_stdio.h"

#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_notify.h"
#endif

struct vrt_reintegrate_info
{
  exa_uuid_t group_uuid;
  char node_name[EXA_MAXSIZE_NODENAME + 1];

  /* complete is true if the rebuilding has finished, false if we need a
   * partial reset-dirty */
  uint64_t complete;

  /* Opaque number sent by the VRT.  Admind must send this number to the VRT on
   * all nodes. Next logical su to rebuild */
  uint64_t next_slot_to_rebuild;
};

/**
 * This function is empty because the VRT runs inside exa_clientd.
 * As a result, its initialization is performed within exa_clientd.
 */
static int
vrt_init (int thr_nb)
{
  return EXA_SUCCESS;
}

static int vrt_suspend(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_vrt,
			     RPC_SERVICE_VRT_SUSPEND, NULL, 0);
}

static int
vrt_suspend_groups(int thr_nb)
{
  struct adm_group *group;

  adm_group_for_each_group(group)
  {
    int ret;

    if (!group->started)
      continue;

    exalog_debug("vrt_client_group_suspend(%s)", group->name);
    ret = vrt_client_group_suspend(adm_wt_get_localmb(), &group->uuid);
    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      exalog_debug("vrt_client_group_suspend(%s) interrupted", group->name);
      return ret;
    }
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "vrt_client_group_suspend(%s): %s",
		       group->name, exa_error_msg(ret));
  }

  return EXA_SUCCESS;
}

static void
local_vrt_suspend(int thr_nb, void *msg)
{
  int ret;

  ret = vrt_suspend_groups(thr_nb);

  admwrk_ack(thr_nb, ret);
}

static int vrt_resume(int thr_nb)
{
   return admwrk_exec_command(thr_nb, &adm_service_vrt, RPC_SERVICE_VRT_RESUME,
			      NULL, 0);
}

static int
vrt_resume_groups(int thr_nb)
{
  struct adm_group *group;

  adm_group_for_each_group(group)
  {
    int ret;

    if (!group->started)
      continue;

    exalog_debug("vrt_client_group_resume(%s)",
		 group->name);
    ret = vrt_client_group_resume(adm_wt_get_localmb(), &group->uuid);
    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      exalog_debug("vrt_client_group_resume(%s) interrupted", group->name);
      return ret;
    }
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "vrt_client_group_resume(%s): %s",
		       group->name, exa_error_msg(ret));
  }

  return EXA_SUCCESS;
}

static void
local_vrt_resume (int thr_nb, void *msg)
{
  int ret;

  ret = vrt_resume_groups(thr_nb);

  admwrk_ack(thr_nb, ret);
}

static int
vrt_recover (int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_vrt,
			     RPC_SERVICE_VRT_RECOVER, NULL, 0);
}

/**
 * Prepare the information needed to create or start a group in the vrt
 * executive.
 *
 * @param[in] group     The group we'll create or start
 *
 * @return 0 if successful, a negative error code otherwise.
 */
int service_vrt_prepare_group(struct adm_group *group)
{
    struct adm_disk *disk;
    int ret;

    ret = vrt_client_group_begin(adm_wt_get_localmb(), group->name,
                                 &group->uuid, vrt_layout_get_name(group->layout),
                                 sb_version_get_version(group->sb_version));
    if (ret != EXA_SUCCESS)
    {
	exalog_error("vrt_client_group_begin(%s): %s",
		     group->name, exa_error_msg(ret));
	return ret;
    }

    /* Get rdevs list */
    adm_group_for_each_disk(group, disk)
    {
        struct adm_node *node = adm_cluster_get_node_by_id(disk->node_id);

        EXA_ASSERT(node != NULL);
	EXA_ASSERT(!disk->up_in_vrt);

	ret = vrt_client_group_add_rdev(adm_wt_get_localmb(), &group->uuid,
					disk->node_id, node->spof_id,
                                        &disk->vrt_uuid, &disk->uuid,
					adm_disk_is_local(disk),
					disk->imported);
	if (ret != EXA_SUCCESS)
	{
	    exalog_error("Failed to add the structure corresponding to device %d:%s "
                         "in the executive: (%i) %s\n",
                         disk->node_id, disk->path, ret, exa_error_msg(ret));
	    return ret;
	}
    }

    return EXA_SUCCESS;
}

int local_exa_dgstart_vrt_start(int thr_nb, struct adm_group *group)
{
    struct adm_disk *disk;
    int ret;

    ret = service_vrt_prepare_group(group);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Failed to prepare group " UUID_FMT ": %s (%d)",
		     UUID_VAL(&group->uuid), exa_error_msg(ret), ret);
	return ret;
    }

    ret = vrt_client_group_start(adm_wt_get_localmb(), &group->uuid);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Failed to start group " UUID_FMT ": %s (%d)",
		     UUID_VAL(&group->uuid), exa_error_msg(ret), ret);
	return ret;
    }

    adm_group_for_each_disk(group, disk)
    {
	EXA_ASSERT(!disk->up_in_vrt);
	disk->up_in_vrt = disk->imported;
    }

    group->started = true;

    group->synched = false;

    return EXA_SUCCESS;
}

static int
vrt_restart_groups(int thr_nb)
{
    struct adm_group *group;

    adm_group_for_each_group(group)
    {
	int ret;

	/* Skip this group if a start is not requested. */
	if (group->goal != ADM_GROUP_GOAL_STARTED)
	    continue;

	/* Skip this group if it is already started. */
	if (group->started)
	{
	    exalog_debug("group %s is already started", group->name);
	    continue;
	}

	ret = local_exa_dgstart_vrt_start(thr_nb, group);
	if (ret == -ADMIND_ERR_NODE_DOWN)
	{
	    exalog_debug("vrt_client_group_start(%s) interrupted", group->name);
	    return ret;
	}

	if (ret != EXA_SUCCESS)
	{
	    exalog_warning("failed to restart %s: %s", group->name, exa_error_msg(ret));
	    continue;
	}
    }

    return EXA_SUCCESS;
}

static int
vrt_update_disks(int thr_nb, struct adm_group *group)
{
    struct adm_disk *disk;

    adm_group_for_each_disk(group, disk)
    {
	int ret;

	/* if unimported disks are already down or imported disks are
	 * already up, there is nothing to do: continue with next disk */
	if (disk->imported)
	{
	    if (disk->up_in_vrt)
		continue;
#ifdef WITH_MONITORING
	    {
	    struct adm_node *node = adm_cluster_get_node_by_id(disk->node_id);
	    /* send a trap to monitoring daemon */
	    md_client_notify_disk_up(adm_wt_get_localmb(),
		    &disk->vrt_uuid,
		    node->name,
		    disk->path);
	    }
#endif
	    ret = vrt_client_device_up(adm_wt_get_localmb(),
				       &group->uuid, &disk->vrt_uuid);
	}
	else
	{
	    if (!disk->up_in_vrt)
		continue;
#ifdef WITH_MONITORING
	    {
	    struct adm_node *node = adm_cluster_get_node_by_id(disk->node_id);
	    /* send a trap to monitoring daemon */
	    md_client_notify_disk_down(adm_wt_get_localmb(),
		    &disk->vrt_uuid,
		    node->name,
		    disk->path);
	    }
#endif
	    ret = vrt_client_device_down(adm_wt_get_localmb(),
		                         &group->uuid, &disk->vrt_uuid);
	}

	if (ret == -ADMIND_ERR_NODE_DOWN)
	    return ret;

	EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
			   "Error while switching device %d:%s to %s: %s (%d)",
			   disk->node_id, disk->path,
			   disk->up_in_vrt ? "DOWN" : "UP",
			   exa_error_msg(ret), ret);

	disk->up_in_vrt = !disk->up_in_vrt; /* toggle disk status */
    }

  return EXA_SUCCESS;
}

static int local_vrt_recover_restart_volume(struct adm_group *group,
                                            struct adm_volume *volume)
{
    int ret = EXA_SUCCESS;

    if (volume->started)
        return EXA_SUCCESS;

    exalog_debug("vrt_client_volume_start(%s:%s)",
                 group->name, volume->name);

    ret = vrt_client_volume_start(adm_wt_get_localmb(),
                                  &group->uuid, &volume->uuid);
    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
        exalog_debug("vrt_client_volume_start(%s:%s) interrupted",
                     group->name, volume->name);
        return ret;
    }
    if (ret == -VRT_ERR_NB_VOLUMES_STARTED)
    {
        exalog_warning("vrt_client_volume_start(%s:%s): %s",
                       group->name, volume->name, exa_error_msg(ret));
        return EXA_SUCCESS;
    }
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
                       "vrt_client_volume_start(%s:%s): %s",
                       group->name, volume->name, exa_error_msg(ret));

    volume->started = true;
    volume->readonly = adm_nodeset_contains_me(&volume->goal_readonly);

    /* FIXME Something is not clear here on the call to _up or _down callback.
     * Having the volume started has nothing to do with the fact that it may
     * return IO errors, but for now, FS layer expects the VRT to tell the
     * resource is down whenever it may return IO errors, making this test
     * strange here... */
    if (!group->offline)
        inst_set_resources_changed_up(&adm_service_lum);
    else
        inst_set_resources_changed_down(&adm_service_lum);

    return EXA_SUCCESS;
}

static int
local_vrt_recover_restart_group_volumes(int thr_nb, struct adm_group *group)
{
    struct adm_volume *volume;

    adm_group_for_each_volume(group, volume)
        if (adm_nodeset_contains_me(&volume->goal_started))
        {
            int ret = local_vrt_recover_restart_volume(group, volume);
            if (ret != EXA_SUCCESS)
            {
                /* XXX Why stop instead of doing best effort and try to
                       start as many volumes as possible? */
                return ret;
            }
        }

    return EXA_SUCCESS;
}

static int local_vrt_resync_group(int thr_nb, struct adm_group *group)
{
    exa_nodeid_t nodeid;
    admwrk_request_t handle;
    int err = EXA_SUCCESS, bcast_err = EXA_SUCCESS, barrier_err;
    bool group_was_synched = false, synched = group->synched;
    exa_nodeset_t nodes_up, nodes_going_up, nodes_going_down;

    inst_get_nodes_up(&adm_service_vrt, &nodes_up);
    inst_get_nodes_going_up(&adm_service_vrt, &nodes_going_up);
    inst_get_nodes_going_down(&adm_service_vrt, &nodes_going_down);

    /* When a recovery up is being done, check if the group was already
     * resynched by a previous recovery up or if it is still needed. */
    if (!exa_nodeset_is_empty(&nodes_going_up))
    {
        admwrk_bcast(thr_nb, &handle, EXAMSG_SERVICE_VRT_RESYNC, &synched, sizeof(synched));
        while (admwrk_get_bcast(&handle, &nodeid, &synched, sizeof(synched), &bcast_err))
        {
            if (bcast_err != EXA_SUCCESS)
                err = bcast_err;

            /* If one node can afford that the resync was done properly on
             * this group, this means that there is no need for a post resync.
             * In the other case, no instance can remember having done a
             * resync (for example at clstart) thus the full resync has
             * to be done. */
            if (synched)
                group_was_synched = true;
        }
    }

    if (err != EXA_SUCCESS)
        return bcast_err;

    if (group->started)
    {
        exa_nodeset_t nodes_to_resync;

        exa_nodeset_reset(&nodes_to_resync);

        if (!exa_nodeset_is_empty(&nodes_going_up))
        {
            /* In recovery up, even no resync was ever done, thus we need
             * to resync for all nodes, even there is just a subset of
             * nodes going up on a group that was not offline, thus the resync
             * was already done a recovery down. */
            if (!group_was_synched)
                adm_nodeset_set_all(&nodes_to_resync);

        }
        else if (!exa_nodeset_is_empty(&nodes_going_down))
        {
            /* When a node goes down, its pending write zones must be
             * resynched */
            exa_nodeset_copy(&nodes_to_resync, &nodes_going_down);
        }

        if (!exa_nodeset_is_empty(&nodes_to_resync))
            err = vrt_client_group_resync(adm_wt_get_localmb(), &group->uuid,
                                          &nodes_to_resync);
        else
            err = EXA_SUCCESS;
    }

    EXA_ASSERT(err == EXA_SUCCESS || err == -VRT_ERR_GROUP_OFFLINE
               || err == -ADMIND_ERR_NODE_DOWN);
    barrier_err = admwrk_barrier(thr_nb, err, "VRT: Resynchronize");

    /* Swallow the error when group is offline. the resync and post resync
     * will be done when group is not offline anymore.
     * FIXME this means that the user can access not resynched data, which
     * may be bad... see bug #4622 */
    if (barrier_err == -VRT_ERR_GROUP_OFFLINE)
    {
        group->synched = false;
        return EXA_SUCCESS;
    }

    if (barrier_err != EXA_SUCCESS)
        return barrier_err;

    if (group->started)
    {
        /* If node is going up, it needs to reload its metadata about pending
         * writes, so it needs to post resync.
         * In case the group was not synched when entering this function, this
         * means that the local instance of the virtualizer may not have its
         * pending write uptodate, thus, we force the post resync. */
        if (!group_was_synched || exa_nodeset_contains(&nodes_going_up, adm_my_id))
            err = vrt_client_group_post_resync(adm_wt_get_localmb(), &group->uuid);
    }

    barrier_err = admwrk_barrier(thr_nb, err, "VRT: Post-resynchronize");
    if (barrier_err == EXA_SUCCESS)
        group->synched = true;

    /* Swallow group offline error and mark group as not synched */
    if (barrier_err == -VRT_ERR_GROUP_OFFLINE)
    {
        group->synched = false;
        return EXA_SUCCESS;
    }

    return barrier_err;
}

static void
local_vrt_recover (int thr_nb, void *msg)
{
  struct adm_group *group;
  exa_nodeset_t nodes_up, nodes_going_up, nodes_going_down;
  int barrier_ret;
  int ret;

  /* Compute the correct mship of service for working
   * FIXME this should probably be a parameter of this function */

  inst_get_nodes_up(&adm_service_vrt, &nodes_up);
  inst_get_nodes_going_up(&adm_service_vrt, &nodes_going_up);
  inst_get_nodes_going_down(&adm_service_vrt, &nodes_going_down);

  exa_nodeset_sum(&nodes_up, &nodes_going_up);
  exa_nodeset_substract(&nodes_up, &nodes_going_down);

  /* Do not print confusing barriers to the user if no group exists. */

  if (!adm_group_get_first())
  {
    ret = vrt_client_nodes_status(adm_wt_get_localmb(), &nodes_up);
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "vrt_client_nodes_status(): %s",
		       exa_error_msg(ret));
    goto local_vrt_recover_end;
  }

  adm_group_for_each_group(group)
  {
      ret = vrt_group_sync_sb_versions(thr_nb, group);
      barrier_ret = admwrk_barrier(thr_nb, ret,
                                   "Synchronizing groups superblock versions.");
      if (barrier_ret != EXA_SUCCESS)
          goto local_vrt_recover_end;
  }

  /* Restart the groups. */

  ret = vrt_restart_groups(thr_nb);
  EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NODE_DOWN);
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Restart the group(s)");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  /* Stop the rebuilding thread. */

  adm_group_for_each_group(group)
  {
    if (!group->started)
      continue;
    ret = vrt_client_group_suspend_metadata_and_rebuild(adm_wt_get_localmb(),
					&group->uuid);
    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      exalog_debug("Rebuild stop interrupted on %s",
		   group->name);
      break;
    }
    EXA_ASSERT(ret == EXA_SUCCESS);
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Stop the rebuilding");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  /* Update nodes status. */

  ret = vrt_client_nodes_status(adm_wt_get_localmb(), &nodes_up);
  EXA_ASSERT(ret == EXA_SUCCESS);
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Set nodes state");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  /* Notify disks' changes. */

  adm_group_for_each_group(group)
  {
    if (!group->started)
      continue;
    ret = vrt_update_disks(thr_nb, group);
    if (ret == -ADMIND_ERR_NODE_DOWN)
      break;
    EXA_ASSERT(ret == EXA_SUCCESS);
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Notify disks' changes");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  /* Compute status the group. */

  adm_group_for_each_group(group)
  {
    if (!group->started)
      continue;

    ret = vrt_client_group_compute_status(adm_wt_get_localmb(),
                                          &group->uuid);
    if (ret == -VRT_WARN_GROUP_OFFLINE)
    {
        group->offline = true;
        ret = EXA_SUCCESS;
    }
    else if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      exalog_debug("vrt_client_group_compute_status(%s) interrupted",
		   group->name);
      break;
    }
    else
        group->offline = false;

    EXA_ASSERT(ret == EXA_SUCCESS);
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Compute status the group(s)");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  /* Wait requests. */

  adm_group_for_each_group(group)
  {
    if (!group->started)
      continue;

    ret = vrt_client_group_wait_initialized_requests(adm_wt_get_localmb(),
        &group->uuid);
    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      exalog_debug("vrt_client_group_wait_initialized_requests(%s) interrupted",
		   group->name);
      break;
    }
    EXA_ASSERT(ret == EXA_SUCCESS);
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Wait requests");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  /* Resync groups */
  adm_group_for_each_group(group)
  {
      ret = local_vrt_resync_group(thr_nb, group);
      if (ret != EXA_SUCCESS)
          goto local_vrt_recover_end;
  }

  adm_group_for_each_group(group)
  {
    ret = adm_vrt_group_sync_sb(thr_nb, group);
    if (ret == -ADMIND_ERR_NODE_DOWN)
      break;
    if (ret == -VRT_ERR_GROUP_NOT_ADMINISTRABLE)
      ret = EXA_SUCCESS;
    EXA_ASSERT(ret == EXA_SUCCESS);
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Commit the superblocks");
  if (barrier_ret != EXA_SUCCESS)
    goto local_vrt_recover_end;

  adm_group_for_each_group(group)
  {
    if (!group->started)
      continue;

    /* Restart the volumes. */
    ret = local_vrt_recover_restart_group_volumes(thr_nb, group);
    if (ret == -ADMIND_ERR_NODE_DOWN)
      break;

    EXA_ASSERT(ret == EXA_SUCCESS);
  }

 local_vrt_recover_end:
  exalog_debug("local_vrt_recover() returned '%s'", exa_error_msg(ret));
  admwrk_ack(thr_nb, ret);
}


struct vrt_reintegrate_synchro
{
  uint32_t reintegrate_needed;
};

static int local_vrt_group_check_up(int thr_nb, struct adm_group *group)
{
    int barrier_ret, ret;
    exa_nodeid_t nodeid;
    int i;
    struct vrt_reintegrate_synchro reintegrate_synchro_local[NBMAX_DISKS_PER_NODE];
    struct vrt_reintegrate_synchro reintegrate_synchro_global[NBMAX_DISKS_PER_GROUP];
    admwrk_request_t handle;
    struct adm_disk *disk;
    int ret_down;
    bool reintegrate_needed;
    int disk_local_index, disk_global_index;
    size_t msg_size = 0;

    memset(reintegrate_synchro_local, 0, sizeof(reintegrate_synchro_local));
    memset(reintegrate_synchro_global, 0, sizeof(reintegrate_synchro_global));

    /* Query the VRT for all local disks in the group if we need to
     * reintegrate the device */
    disk_local_index = -1;
    adm_group_for_each_disk(group, disk)
    {
        struct vrt_realdev_reintegrate_info reply;
        exa_nodeset_t nodes_up;

        /* reintegrate only local disk on started groups */
        if (disk->node_id != adm_my_id)
            continue;

        disk_local_index++;

        /* FIXME the test below is quite strange: it would mean that the
         * checkup is being performed on a node that is not UP ? looks like
         * some uncontrolled side effect swallow */
        inst_get_nodes_up(&adm_service_vrt, &nodes_up);
        if (!exa_nodeset_contains(&nodes_up, adm_my_id))
            continue;

        if (!group->started)
            continue;

        exalog_debug("vrt_client_rdev_reintegrate_info(%s, %d:%s)",
                group->name, disk->node_id, disk->path);
        ret = vrt_client_rdev_reintegrate_info(adm_wt_get_localmb(),
                &group->uuid,
                &disk->vrt_uuid, &reply);
        if (ret == -VRT_ERR_UNKNOWN_GROUP_UUID)
        {
            /* When group does not exist anymore, swallow the error
             * see bug #4590 */
            disk_local_index = 0;
            msg_size = 0;
            break;
        }

        EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "vrt_client_rdev_reintegrate_info(%s, %d:%s): %s",
                group->name, disk->node_id, disk->path, exa_error_msg(ret));

        EXA_ASSERT(disk_local_index < NBMAX_DISKS_PER_NODE);
        reintegrate_synchro_local[disk_local_index].reintegrate_needed = reply.reintegrate_needed;
        msg_size += sizeof(reintegrate_synchro_local[disk_local_index]);
    }

    /* all nodes send their reintegrate info about their own disks */
    COMPILE_TIME_ASSERT(sizeof(reintegrate_synchro_global) <=
            ADM_MAILBOX_PAYLOAD_PER_NODE * EXA_MAX_NODES_NUMBER);
    admwrk_bcast(thr_nb, &handle, EXAMSG_SERVICE_VRT_REINTEGRATE_INFO,
            reintegrate_synchro_local, msg_size);
    ret = EXA_SUCCESS;
    while (admwrk_get_bcast(&handle, &nodeid, reintegrate_synchro_local,
                sizeof(reintegrate_synchro_local), &ret_down))
    {
        if (ret_down != EXA_SUCCESS && ret_down != -ADMIND_ERR_NODE_DOWN)
            ret = ret_down;

        if (ret_down != EXA_SUCCESS)
            continue;

        disk_global_index = -1;
        disk_local_index = -1;
        adm_group_for_each_disk(group, disk)
        {
            disk_global_index++;
            if (nodeid == disk->node_id)
            {
                disk_local_index++;
                memcpy(& reintegrate_synchro_global[disk_global_index],
                        & reintegrate_synchro_local[disk_local_index],
                        sizeof(reintegrate_synchro_global[disk_global_index]));
            }
        }
    }
    if (ret != EXA_SUCCESS)
        return ret;

    /* all nodes know the reintegrate info of all disks on all nodes*/

    /* find if at least 1 disk need a reintegrate */
    reintegrate_needed = false;
    for (i = 0 ; i < NBMAX_DISKS_PER_GROUP ; i++)
    {
        if (reintegrate_synchro_global[i].reintegrate_needed)
        {
            reintegrate_needed = true;
            break;
        }
    }

    /* if nobody want a reintegrate, skip */
    if (!reintegrate_needed)
        return EXA_SUCCESS;

    /* freeze the group */
    if (group->started)
        ret = vrt_client_group_freeze(adm_wt_get_localmb(), &group->uuid);
    else
        ret = EXA_SUCCESS;

    barrier_ret = admwrk_barrier(thr_nb, ret, "Freeze group for reintegrate");
    if (barrier_ret != EXA_SUCCESS)
        return barrier_ret;

    /* reintegrate all disks */
    disk_global_index = -1;
    adm_group_for_each_disk(group, disk)
    {
        disk_global_index++;

        if (!reintegrate_synchro_global[disk_global_index].reintegrate_needed)
            continue;

        /* Reset dirty zones */
        if (group->started)
            ret = vrt_client_device_reintegrate(adm_wt_get_localmb(),
                    &group->uuid, &disk->vrt_uuid);
        else
            ret = EXA_SUCCESS;

        EXA_ASSERT(ret == EXA_SUCCESS || ret == -VRT_ERR_UNKNOWN_GROUP_UUID
                || ret == -ADMIND_ERR_NODE_DOWN);

        if (ret != EXA_SUCCESS)
            break;
    }

    barrier_ret = admwrk_barrier(thr_nb, ret, "Reintegrating devices");
    if (barrier_ret != EXA_SUCCESS)
    {
        EXA_ASSERT(vrt_client_group_unfreeze(adm_wt_get_localmb(), &group->uuid) == EXA_SUCCESS);
        return barrier_ret;
    }

    /* post-reintegrate all disks */
    disk_global_index = -1;
    adm_group_for_each_disk(group, disk)
    {
        disk_global_index++;

        if (!reintegrate_synchro_global[disk_global_index].reintegrate_needed)
            continue;

        /* Post-Reintegrate device. */
        if (group->started)
            ret = vrt_client_device_post_reintegrate(adm_wt_get_localmb(),
                    &group->uuid, &disk->vrt_uuid);
        else
            ret = EXA_SUCCESS;

        EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NODE_DOWN,
                "Post integration of disk '" UUID_FMT "' failed: %s (%d)",
                UUID_VAL(&disk->vrt_uuid), exa_error_msg(ret), ret);

        if (ret != EXA_SUCCESS)
            break;
    }


    barrier_ret = admwrk_barrier(thr_nb, ret, "Post-reintegrating device");
    /* FIXME: we should leave even if the group is not started*/
    if (barrier_ret != EXA_SUCCESS && group->started)
    {
        EXA_ASSERT(vrt_client_group_unfreeze(adm_wt_get_localmb(), &group->uuid) == EXA_SUCCESS);
        return barrier_ret;
    }

    /* unfreeze the group */
    if (group->started)
    {
        ret = vrt_client_group_unfreeze(adm_wt_get_localmb(), &group->uuid);
        EXA_ASSERT(ret == EXA_SUCCESS);
    }

    /* Sync SB. */
    ret = adm_vrt_group_sync_sb(thr_nb, group);
    if (ret == -VRT_ERR_GROUP_NOT_ADMINISTRABLE)
        ret = EXA_SUCCESS;

    EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NODE_DOWN);

    return admwrk_barrier(thr_nb, ret, "Syncing superblocks on disk");
}

static void local_vrt_check_up(int thr_nb, void *msg)
{
  struct adm_group *group;
  int ret = EXA_SUCCESS;

  adm_group_for_each_group(group)
  {
      ret = local_vrt_group_check_up(thr_nb, group);
      if (ret != EXA_SUCCESS)
          break;
  }

  admwrk_ack(thr_nb, ret);
}

/*------------------------------------------------------------------------------*/
/** \brief shutdown daemons.
 *
 * \param [in] msg: message receive.
 * \return EXA_SUCCESS or negative error code : success/error.
 */
/*------------------------------------------------------------------------------*/
static int
vrt_shutdown(int thr_nb)
{
    return vrt_client_pending_group_cleanup(adm_wt_get_localmb());
}

static int
vrt_check_up(int thr_nb)
{
  int ret = admwrk_exec_command(thr_nb, &adm_service_vrt,
                                RPC_SERVICE_VRT_CHECK_UP, NULL, 0);
  if (ret != EXA_SUCCESS)
    exalog_warning("Vrt check up failed: %s (%d)", exa_error_msg(ret), ret);

  /* Ignore checkup that would fail on a unknown group because a race exists
   * with dgstop see bug #4590 */
  if (ret == -VRT_ERR_UNKNOWN_GROUP_UUID)
      ret = EXA_SUCCESS;

  return ret;
}

int service_vrt_group_stop(int thr_nb, struct adm_group *group, bool force)
{
    int ret = EXA_SUCCESS;

    if (force || group->started)
    {
	ret = vrt_client_group_stop(adm_wt_get_localmb(), &group->uuid);

	if (ret == -VRT_ERR_GROUP_NOT_STARTED)
	    ret = EXA_SUCCESS;

	if (force || ret == EXA_SUCCESS)
	{
	    struct adm_disk *disk;
	    group->started = false;
	    group->synched = false;
	    adm_group_for_each_disk(group, disk)
		disk->up_in_vrt = false;
	}
    }

    if (force)
	ret = EXA_SUCCESS;

    return ret;
}

static int
vrt_nodestop_stop_groups(int thr_nb, bool force)
{
  struct adm_group *group;

  adm_group_for_each_group(group)
  {
    int ret = service_vrt_group_stop(thr_nb, group, force);
    if (ret)
	return ret;
  }

  return EXA_SUCCESS;
}


static int
vrt_nodestop(int thr_nb, const exa_nodeset_t *nodes_to_stop,
	     bool goal_change, bool force)
{
  struct adm_group *group;
  exa_nodeset_t nodes_up;
  int ret = EXA_SUCCESS;
  int barrier_ret;
  char buf[64];

  /* Compute the set of instances that will remain up after this stop. */
  inst_get_nodes_up(&adm_service_vrt, &nodes_up);
  exa_nodeset_substract(&nodes_up, nodes_to_stop);

  /* Mark all groups with goal stopped if requested;
   * No need to check if all nodes are involved, this is already done in
   * clnodestop command. */
  if (goal_change)
  {
      /*No need to save the config file here, is is done by admin service
       * when marking its own goal to stopped */
      /* FIXME It would be really nice to handle the goal separatly from the
       * stop command itself. A cluster command that would pass goal to stop
       * for groups could be factorized with dgstop. */
      adm_group_for_each_group(group)
	  group->goal = ADM_GROUP_GOAL_STOPPED;
  }

  /* Stop the groups on the nodes to stop. */
  if (adm_nodeset_contains_me(nodes_to_stop))
    ret = vrt_nodestop_stop_groups(thr_nb, force);
  else
    ret = -ADMIND_ERR_NOTHINGTODO;
  EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NOTHINGTODO);
  barrier_ret = admwrk_barrier(thr_nb, ret, "Stop the groups on stopping nodes");
  if (barrier_ret != EXA_SUCCESS)
    return barrier_ret;

  /* Stop the rebuilding thread. */
  adm_group_for_each_group(group)
  {
    os_snprintf(buf, sizeof(buf), "Stop rebuilding on '%s'", group->name);

    if (group->started &&
	!adm_nodeset_contains_me(nodes_to_stop))
      ret = vrt_client_group_suspend_metadata_and_rebuild(adm_wt_get_localmb(),
					  &group->uuid);
    else
      ret = -ADMIND_ERR_NOTHINGTODO;

    EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NOTHINGTODO);
    barrier_ret = admwrk_barrier(thr_nb, ret, buf);
    if (barrier_ret != EXA_SUCCESS)
      return barrier_ret;
  }

  /* Update nodes status. */
  ret = vrt_client_nodes_status(adm_wt_get_localmb(), &nodes_up);
  EXA_ASSERT(ret == EXA_SUCCESS);
  barrier_ret = admwrk_barrier(thr_nb, ret, "Set nodes' state");
  if (barrier_ret != EXA_SUCCESS)
    return barrier_ret;

  /* Notify disks' changes. */
  adm_group_for_each_group(group)
  {
    os_snprintf(buf, sizeof(buf), "Notify disks' changes of '%s'", group->name);

    ret = -ADMIND_ERR_NOTHINGTODO;

    if (group->started && !adm_nodeset_contains_me(nodes_to_stop))
    {
	struct adm_disk *disk;

	adm_group_for_each_disk(group, disk)
	{
	    /* if the disk is up in vrt and belongs to a node that we are
	     * stopping, we have to tell the VRT to stop using it. (else
	     * continue with next disk)*/
	    if (!disk->up_in_vrt
		|| !exa_nodeset_contains(nodes_to_stop, disk->node_id))
		continue;

	    ret = vrt_client_device_down(adm_wt_get_localmb(),
					 &group->uuid, &disk->vrt_uuid);

	    if (ret != EXA_SUCCESS)
		break;

	    disk->up_in_vrt = false;
	}
    }

    EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NOTHINGTODO);
    barrier_ret = admwrk_barrier(thr_nb, ret, buf);
    if (barrier_ret != EXA_SUCCESS)
      return barrier_ret;
  }

  /* Compute status */
  adm_group_for_each_group(group)
  {
    os_snprintf(buf, sizeof(buf), "Compute new status of '%s'", group->name);

    if (group->started && !adm_nodeset_contains_me(nodes_to_stop))
    {
        ret = vrt_client_group_compute_status(adm_wt_get_localmb(), &group->uuid);
        if (ret == -VRT_WARN_GROUP_OFFLINE)
        {
            group->offline = true;
            ret = EXA_SUCCESS;
        }
    }
    else
      ret = -ADMIND_ERR_NOTHINGTODO;

    EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NOTHINGTODO);
    barrier_ret = admwrk_barrier(thr_nb, ret, buf);
    if (barrier_ret != EXA_SUCCESS)
      return barrier_ret;
  }

  /* Wait requests. */
  adm_group_for_each_group(group)
  {
    if (group->started &&
	!adm_nodeset_contains_me(nodes_to_stop))
    {
      ret = vrt_client_group_wait_initialized_requests(adm_wt_get_localmb(),
          &group->uuid);
      if (ret == -ADMIND_ERR_NODE_DOWN)
      {
        exalog_debug("vrt_client_group_wait_initialized_requests(%s) interrupted",
                     group->name);
        break;
      }
      EXA_ASSERT(ret == EXA_SUCCESS);
    }
    else
      ret = -ADMIND_ERR_NOTHINGTODO;
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "VRT: Wait requests");
  if (barrier_ret != EXA_SUCCESS)
    return barrier_ret;

  adm_group_for_each_group(group)
  {
    /* Sync the superblocks on one node that won't be stopped. */
    ret = adm_vrt_group_sync_sb(thr_nb, group);
    if (ret == -VRT_ERR_GROUP_NOT_ADMINISTRABLE)
        ret = EXA_SUCCESS;
    EXA_ASSERT(ret == EXA_SUCCESS
               || ret == -ADMIND_ERR_NOTHINGTODO
               || ret == -ADMIND_ERR_NODE_DOWN);

    os_snprintf(buf, sizeof(buf), "Synchronize superblocks of '%s'", group->name);
    barrier_ret = admwrk_barrier(thr_nb, ret, buf);
    if (barrier_ret != EXA_SUCCESS)
      return barrier_ret;
  }

  barrier_ret = admwrk_barrier(thr_nb, ret, "Reload the volumes");
  if (barrier_ret != EXA_SUCCESS)
    return barrier_ret;

  return EXA_SUCCESS;
}

static void local_vrt_stop(int thr_nb, void *msg)
{
  const exa_nodeset_t *nodeset = &((const stop_data_t *)msg)->nodes_to_stop;
  bool goal_change = ((const stop_data_t *)msg)->goal_change;
  bool force = ((const stop_data_t *)msg)->force;

  int ret = vrt_nodestop(thr_nb, nodeset, goal_change, force);

  admwrk_ack(thr_nb, ret);
}

/**
 * Stop all volumes in a group.
 *
 * @param thr_nb       the thread number
 * @param group        the group in which volumes to stop are.
 * @param nodelist     the nodes on which to stop the volumes.
 * @param force        try to force execution even if in bad state.
 * @param goal_change  specify whether we need to change the volume's goal
 *
 * @return EXA_SUCCESS or an error code on first failing volume.
 *
 * NOTE: whenever an error occurs, some volumes may remain started on some
 *       nodes because there is no purpose for rollback.
 */
static int vrt_stop_all_volumes(int thr_nb, struct adm_group *group,
                                const exa_nodeset_t *nodelist, bool force,
                                adm_goal_change_t goal_change)
{
    struct adm_volume *volume;

    EXA_ASSERT(group != NULL);
    EXA_ASSERT(nodelist != NULL);

    adm_group_for_each_volume(group, volume)
    {
        int err = vrt_master_volume_stop(thr_nb, volume, nodelist, force,
                                         goal_change, false /* print_warning */);

        if (err != EXA_SUCCESS)
            return err;
    }

    return EXA_SUCCESS;
}

static int vrt_stop(int thr_nb, const stop_data_t *stop_data)
{
  struct adm_group *group;

  adm_group_for_each_group(group)
  {
      /* FIXME this kind of bitfield is really evil and useless.. why not
       * simply pass arguments to function.... */
      adm_goal_change_t goal_change = stop_data->goal_change ?
	  ADM_GOAL_CHANGE_GROUP | ADM_GOAL_CHANGE_VOLUME : ADM_GOAL_PRESERVE;

      int ret = vrt_stop_all_volumes(thr_nb, group, &stop_data->nodes_to_stop,
	                             stop_data->force, goal_change);

      if (ret != EXA_SUCCESS)
	  return ret;
  }

  return admwrk_exec_command(thr_nb, &adm_service_vrt, RPC_SERVICE_VRT_STOP,
                             stop_data, sizeof(*stop_data));
}

static void
vrt_nodedel(int thr_nb, struct adm_node *node)
{
    struct adm_group *group;
    struct adm_volume *volume;

    adm_group_for_each_group(group)
        adm_group_for_each_volume(group, volume)
            exa_nodeset_del(&volume->goal_stopped, node->id);
}


static int vrt_check_nodedel(int thr_nb, struct adm_node *node)
{
  struct adm_group *group;
  struct adm_volume *volume;

  /* If the node to delete has a goal 'started' on a volume, we are
   * not allowed to delete it.
   */
  adm_group_for_each_group(group)
    adm_group_for_each_volume(group, volume)
        if (exa_nodeset_contains(&volume->goal_started, node->id))
            return -VRT_ERR_CANNOT_DELETE_NODE;

  return EXA_SUCCESS;
}

const struct adm_service adm_service_vrt =
{
  .id = ADM_SERVICE_VRT,
  .init = vrt_init,
  .recover = vrt_recover,
  .resume = vrt_resume,
  .suspend = vrt_suspend,
  .shutdown = vrt_shutdown,
  .check_up = vrt_check_up,
  .stop = vrt_stop,
  .nodedel = vrt_nodedel,
  .check_nodedel = vrt_check_nodedel,
  .local_commands =
  {
    { RPC_SERVICE_VRT_SUSPEND,                  local_vrt_suspend              },
    { RPC_SERVICE_VRT_RESUME,                   local_vrt_resume               },
    { RPC_SERVICE_VRT_RECOVER,                  local_vrt_recover              },
    { RPC_SERVICE_VRT_CHECK_UP,                 local_vrt_check_up             },
    { RPC_SERVICE_VRT_STOP,                     local_vrt_stop                 },
    { RPC_COMMAND_NULL, NULL }
  }
};
