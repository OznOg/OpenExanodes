/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/instance.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_nodeset.h"
#include "log/include/log.h"
#include "os/include/os_disk.h"
#include "os/include/strlcpy.h"

#include <string.h>

__export(EXA_ADM_CLDISKDEL) struct cldiskdel_params
  {
    __optional char node_name[EXA_MAXSIZE_NODENAME + 1] __default("");
    __optional char disk_path[EXA_MAXSIZE_DEVPATH + 1] __default("");
    __optional char uuid[UUID_STR_LEN + 1] __default("");
  };

struct msg_diskdel
{
  exa_uuid_t uuid;
};

static void
cluster_cldiskdel(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct cldiskdel_params *params = data;
  struct adm_disk *disk;
  struct adm_node *disk_node;
  struct msg_diskdel msg;
  exa_nodeset_t nodes_up;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (params->node_name[0] == '\0'
      && params->disk_path[0] == '\0'
      && params->uuid[0] == '\0')
  {
    set_error(err_desc, -EXA_ERR_INVALID_PARAM,
	      "Should specify at least one of nodename and rdevpath "
	      "parameters or uuid parameter");
    return;
  }

  /* FIXME Aren't these conditions b0rked? Or, at least, aren't the error
   * messages not matching the actual conditions?? */
  if (params->node_name[0] != '\0' && params->disk_path[0] == '\0')
  {
    set_error(err_desc, -EXA_ERR_INVALID_PARAM,
	      "Could not find node for disk to delete");
    return;
  }

  if (params->node_name[0] == '\0' && params->disk_path[0] != '\0')
  {
    set_error(err_desc, -EXA_ERR_INVALID_PARAM,
	      "Could not find path for disk to delete");
    return;
  }

  if (params->node_name[0] != '\0' && params->uuid[0] != '\0')
  {
    set_error(err_desc, -EXA_ERR_INVALID_PARAM,
              "Should specify only one of nodename and rdevpath"
	      " parameters or uuid parameter");
    return;
  }

  if (params->uuid[0] != '\0')
  {
    exa_uuid_t uuid;
    uuid_scan(params->uuid, &uuid);

    disk = adm_cluster_get_disk_by_uuid(&uuid);
    if (disk == NULL)
    {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK,
	        "Could not find disk with UUID='%s'", params->uuid);
      return;
    }
  }
  else
  {
    char path[EXA_MAXSIZE_DEVPATH + 1];

    os_disk_normalize_path(params->disk_path, path, sizeof(path));
    disk = adm_cluster_get_disk_by_path(params->node_name, path);
    if (disk == NULL)
    {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK,
	        "Could not find disk '%s:%s'",
		params->node_name, params->disk_path);
      return;
    }
  }

  /* We can assert we have a disk, because either we had an UUID, either we
   * had node_name + path, and in both cases if the disk wasn't found, an
   * error has been returned already.
   */
  EXA_ASSERT(disk != NULL);
  disk_node = adm_cluster_get_node_by_id(disk->node_id);
  EXA_ASSERT(disk_node != NULL);

  inst_get_nodes_up(&adm_service_rdev, &nodes_up);
  if (!exa_nodeset_contains(&nodes_up, disk_node->id))
  {
      set_error(err_desc, -ADMIND_ERR_NODE_DOWN, "Node '%s' is down",
                disk_node->name);
      return;
  }

  if (!uuid_is_zero(&disk->group_uuid))
    {
      struct adm_group *group = adm_group_get_group_by_uuid(&disk->group_uuid);
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
		"Can't remove disk '%s:%s': currently used by group '%s'",
		disk_node->name, disk->path, group->name);
      return;
    }

  exalog_info("received cldiskdel --disk %s:%s from %s",
	      disk_node->name, disk->path, adm_cli_ip());

  msg.uuid = disk->uuid;

  admwrk_exec_command(ctx, &adm_service_rdev,
		      RPC_ADM_CLDISKDEL, &msg, sizeof(msg));

  set_success(err_desc);
}


static void
local_diskdel(admwrk_ctx_t *ctx, void *msg)
{
  struct msg_diskdel *request = msg;
  struct adm_disk *disk;
  struct adm_node *node;
  const struct adm_service *service;

  disk = adm_cluster_get_disk_by_uuid(&request->uuid);
  EXA_ASSERT(disk != NULL);

  node = adm_cluster_get_node_by_id(disk->node_id);
  EXA_ASSERT(node != NULL);

  adm_node_remove_disk(node, disk);
  adm_cluster_remove_disk();

  adm_service_for_each_reverse(service)
    if (service->diskdel != NULL)
      service->diskdel(ctx, node, disk);

  EXA_ASSERT(conf_save_synchronous() == EXA_SUCCESS);

  exalog_debug("removed disk %s:%s ", node->name, disk->path);

  adm_disk_delete(disk);

  admwrk_ack(ctx, EXA_SUCCESS);
}


const AdmCommand exa_cldiskdel = {
  .code            = EXA_ADM_CLDISKDEL,
  .msg             = "cldiskdel",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_cldiskdel,
  .local_commands  = {
     { RPC_ADM_CLDISKDEL, local_diskdel },
     { RPC_COMMAND_NULL, NULL }
   }
};

