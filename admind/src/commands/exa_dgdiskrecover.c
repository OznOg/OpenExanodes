/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/include/evmgr_pub_events.h"
#include "admind/include/service_vrt.h"
#include "admind/services/rdev/include/rdev.h"
#include "admind/src/commands/exa_clinfo_node.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/instance.h"
#include "admind/src/evmgr/evmgr.h"

#include "nbd/service/include/nbdservice_client.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#include "log/include/log.h"

#include "os/include/os_disk.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

__export(EXA_ADM_DGDISKRECOVER) struct dgdiskrecover_params
  {
    char old_disk[UUID_STR_LEN + 1];
    char new_disk[UUID_STR_LEN + 1];
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  };

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct dgdiskrecover_info
{
  exa_nodeid_t node_id;
  exa_uuid_t group_uuid;
  exa_uuid_t old_disk_uuid;
  exa_uuid_t new_disk_uuid;
};

/**
 * dgdiskrecover cluster command.
 *
 * The dgdiskrecover cluster commands basically reads the arguments from
 * the XML description of the command, and builds a struct
 * dgdiskrecover_info message used to pass the arguments to the local
 * commands.
 */
static void
cluster_dgdiskrecover(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct dgdiskrecover_params *params = data;
  struct adm_group *group;
  struct adm_disk *old_disk, *new_disk;
  exa_uuid_t old_disk_uuid, new_disk_uuid;
  int ret, err;
  struct dgdiskrecover_info info;
  exa_nodeset_t nodes_up;
  struct disk_info_reply reply;
  struct disk_info_query query;
  exa_nodeid_t nodeid;
  exa_nodeid_t new_disk_nodeid;
  admwrk_request_t rpc;
  bool new_disk_ok;

  memset(&info, 0, sizeof(info));

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (adm_cluster.goal != ADM_CLUSTER_GOAL_STARTED)
  {
      set_error(err_desc, -EXA_ERR_ADMIND_STOPPED, NULL);
      return;
  }

  if (uuid_scan(params->old_disk, &old_disk_uuid) < 0)
  {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, "Invalid old disk UUID");
      return;
  }

  if (uuid_scan(params->new_disk, &new_disk_uuid) < 0)
  {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, "Invalid new disk UUID");
      return;
  }

  exalog_info("received dgdiskrecover '%s' --old "UUID_FMT " --new "UUID_FMT
              " from %s", params->group_name, UUID_VAL(&old_disk_uuid),
              UUID_VAL(&new_disk_uuid), adm_cli_ip());

  group = adm_group_get_group_by_name(params->group_name);
  if (group == NULL)
  {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME, NULL);
      return;
  }

  old_disk = adm_group_get_disk_by_uuid(group, &old_disk_uuid);
  if (old_disk == NULL)
  {
    set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK_UUID,
              "No disk with UUID "UUID_FMT" in group '%s'",
              UUID_VAL(&old_disk_uuid), params->group_name);
    return;
  }

  new_disk = adm_cluster_get_disk_by_uuid(&new_disk_uuid);
  if (new_disk == NULL)
  {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK_UUID,
                "No disk with UUID "UUID_FMT" in cluster",
                UUID_VAL(&new_disk_uuid));
      return;
  }

  if (old_disk->node_id != new_disk->node_id)
  {
      set_error(err_desc, -ADMIND_ERR_MOVED_DISK, "Old disk "UUID_FMT" and "
                "new disk "UUID_FMT" are not on the same node.",
                UUID_VAL(&old_disk->uuid), UUID_VAL(&new_disk->uuid));
      return;
  }

  if (!uuid_is_zero(&new_disk->group_uuid))
  {
      /* FIXME Use proper error code */
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK_UUID,
                "Disk "UUID_FMT" is not unassigned", UUID_VAL(&new_disk_uuid));
      return;
  }

  /* Do not continue if the group is not started */
  if (group->goal == ADM_GROUP_GOAL_STOPPED)
  {
      set_error(err_desc, -VRT_ERR_GROUP_NOT_STARTED,
                "Group '%s' is not started", group->name);
      return;
  }

  /* Check that the node hosting the old disk is up. No need to check the
     node hosting the new disk, since currently a disk can only be replaced
     by another one on the same node. */
  inst_get_nodes_up(&adm_service_rdev, &nodes_up);
  if (!exa_nodeset_contains(&nodes_up, old_disk->node_id))
  {
        set_error(err_desc, -VRT_ERR_RDEV_DOWN, "Cannot recover disk: its node is down");
        return;
  }

  uuid_copy(&query.uuid, &new_disk_uuid);

  new_disk_ok = false;
  new_disk_nodeid = EXA_NODEID_NONE;

  admwrk_run_command(ctx, &adm_service_rdev, &rpc,
          RPC_ADM_CLINFO_DISK_INFO, &query, sizeof(query));

  /* Only one reply will contain the local information about the disk. */
  while (admwrk_get_reply(&rpc, &nodeid, &reply, sizeof(reply), &err))
  {
      if (err == -ADMIND_ERR_NODE_DOWN)
        continue;
      if (!reply.has_disk)
          continue;

      EXA_ASSERT(uuid_is_equal(&reply.uuid, &new_disk_uuid));
      EXA_ASSERT_VERBOSE(new_disk_nodeid == EXA_NODEID_NONE,
                         "More than one node reported disk "UUID_FMT" as local",
                         UUID_VAL(&new_disk_uuid));
      new_disk_nodeid = nodeid;

      if (!strcmp(reply.status, ADMIND_PROP_UP))
          new_disk_ok = true;
      else
        set_error(err_desc, -ADMIND_ERR_WRONG_DISK_STATUS,
                "Disk "UUID_FMT" is %s", UUID_VAL(&new_disk_uuid), reply.status);
  }

  EXA_ASSERT(new_disk_nodeid != EXA_NODEID_NONE);

  if (!new_disk_ok)
      return;

  info.node_id = old_disk->node_id;
  uuid_copy(&info.group_uuid, &group->uuid);
  uuid_copy(&info.old_disk_uuid, &old_disk_uuid);
  uuid_copy(&info.new_disk_uuid, &new_disk_uuid);

  ret = admwrk_exec_command(ctx, &adm_service_rdev, RPC_ADM_DGDISKRECOVER,
                            &info, sizeof(info));
  if (ret != EXA_SUCCESS)
    {
      set_error(err_desc, ret, NULL);
      return;
    }

  evmgr_request_recovery(adm_wt_get_localmb());

  set_success(err_desc);
}


/**
 * The dgdiskrecover local command, executed on all nodes.
 */
static void
local_dgdiskrecover(admwrk_ctx_t *ctx, void *msg)
{
  int ret;
  int barrier_ret;
  struct dgdiskrecover_info *info = msg;
  struct adm_group *group;
  struct adm_disk *old_disk, *new_disk;

  /* Node, group and disk existence have already been verified in the
     cluster command, so we just assert if either of them is not found. */
  group = adm_group_get_group_by_uuid(&info->group_uuid);
  EXA_ASSERT(group);

  old_disk = adm_group_get_disk_by_uuid(group, &info->old_disk_uuid);
  EXA_ASSERT(old_disk);

  new_disk = adm_cluster_get_disk_by_uuid(&info->new_disk_uuid);
  EXA_ASSERT(new_disk);

  uuid_copy(&new_disk->vrt_uuid, &old_disk->vrt_uuid);

  ret = vrt_client_device_replace(adm_wt_get_localmb(), &group->uuid,
                                  &new_disk->vrt_uuid, &new_disk->uuid);

  barrier_ret = admwrk_barrier(ctx, ret, "Replacing the disk");
  if (barrier_ret != EXA_SUCCESS)
      goto local_exa_dgdiskrecover_end;

  /* Synchronize the group SBs */
  ret = adm_vrt_group_sync_sb(ctx, group);

  barrier_ret = admwrk_barrier(ctx, ret, "Synchronyse the group SBs");
  if (barrier_ret != EXA_SUCCESS)
      goto local_exa_dgdiskrecover_end;

  adm_group_remove_disk(group, old_disk);
  ret = adm_group_insert_disk(group, new_disk);

  /* Assert is not really nice, but rollback may not be possible...
   * this would need some transactional behaviour */
  EXA_ASSERT(ret == EXA_SUCCESS);

  ret = conf_save_synchronous();
  EXA_ASSERT(ret == EXA_SUCCESS);

  inst_set_resources_changed_up(&adm_service_vrt);

local_exa_dgdiskrecover_end:
  admwrk_ack(ctx, barrier_ret);
}


/**
 * Definition of the dgdiskrecover command.
 */
const AdmCommand exa_dgdiskrecover = {
  .code            = EXA_ADM_DGDISKRECOVER,
  .msg             = "dgdiskrecover",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_dgdiskrecover,
  .local_commands = {
    { RPC_ADM_DGDISKRECOVER, local_dgdiskrecover },
    { RPC_COMMAND_NULL, NULL }
  }
};
