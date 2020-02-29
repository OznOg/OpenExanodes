/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>

#include "admind/services/rdev/include/rdev.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "log/include/log.h"
#include "os/include/os_disk.h"
#include "os/include/os_file.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"

__export(EXA_ADM_CLDISKADD) struct cldiskadd_params
  {
    char node_name[EXA_MAXSIZE_NODENAME + 1];
    char disk_path[EXA_MAXSIZE_DEVPATH + 1];
  };

struct msg_diskadd
{
  exa_nodeid_t node;
  char path[EXA_MAXSIZE_DEVPATH + 1];
  exa_uuid_t uuid;
};

static void
cluster_cldiskadd(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct cldiskadd_params *params = data;
  exa_nodeset_t nodes_up;
  struct msg_diskadd msg;
  const struct adm_node *node = NULL;
  struct adm_disk *disk;
  int ret;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  node = adm_cluster_get_node_by_name(params->node_name);
  if (node == NULL)
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, "No such node '%s'",
		params->node_name);
      return;
    }
  msg.node = node->id;

  /*
   * The two checks below are a workaround. They are duplicate, too early and
   * not atomic with the insert. They should be removed and local_diskadd()
   * should handle errors instead of asserting.
   */

  if (adm_cluster_nb_disks() == NBMAX_DISKS)
  {
    set_error(err_desc, -ADMIND_ERR_TOO_MANY_DISKS,
              "Too many disks in the cluster (max %u)", NBMAX_DISKS);
    return;
  }

  if (adm_node_nb_disks(node) == NBMAX_DISKS_PER_NODE)
  {
    set_error(err_desc, -ADMIND_ERR_TOO_MANY_DISKS_IN_NODE,
              "Too many disks in node '%s' (max %u)",
              NBMAX_DISKS_PER_NODE, params->node_name);
    return;
  }

  inst_get_nodes_up(&adm_service_rdev, &nodes_up);
  if(!exa_nodeset_contains(&nodes_up, node->id))
  {
    set_error(err_desc, -EXA_ERR_INVALID_PARAM, "Node '%s' is down",
              params->node_name);
    return;
  }

  os_disk_normalize_path(params->disk_path, msg.path, sizeof(msg.path));

  adm_node_for_each_disk(node, disk)
    if (strncmp(disk->path, msg.path, sizeof(disk->path)) == 0)
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
		"Device path '%s' already exported", msg.path);
      return;
    }

  exalog_info("received cldiskadd --disk %s:%s  from %s",
	      node->name, msg.path, adm_cli_ip());

  uuid_generate(&msg.uuid);

  ret = admwrk_exec_command(thr_nb, &adm_service_rdev,
			    RPC_ADM_CLDISKADD, &msg, sizeof(msg));

  set_error(err_desc, ret, NULL);
}


static void
local_diskadd(int thr_nb, void *msg)
{
  struct msg_diskadd *request = msg;
  char step[EXA_MAXSIZE_LINE + 1] = "";
  const struct adm_node *node;
  struct adm_disk *disk;
  int rv;
  int i;

  node = adm_cluster_get_node_by_id(request->node);
  EXA_ASSERT(node != NULL);

  disk = adm_disk_alloc();
  EXA_ASSERT(disk != NULL);

  disk->uuid = request->uuid;
  disk->node_id = node->id;

  if (node == adm_myself())
    rv = adm_disk_local_new(disk);
  else
    rv = EXA_SUCCESS;

  os_snprintf(step, EXA_MAXSIZE_LINE + 1, "Get info about %s", disk->path);

  rv = admwrk_barrier(thr_nb, rv, step);

  if (rv != EXA_SUCCESS)
  {
    adm_disk_delete(disk);
    goto end;
  }

  if (node == adm_myself())
    rv = rdev_initialize_sb(request->path, &disk->uuid);
  else
    rv = EXA_SUCCESS;

  os_snprintf(step, EXA_MAXSIZE_LINE + 1, "Initialize %s", disk->path);

  rv = admwrk_barrier(thr_nb, rv, step);

  if (rv != EXA_SUCCESS)
  {
    /* There is no need to rollback the diskadd() callback since it is
       done only on one node (wich already rollbacked it). */
    adm_disk_delete(disk);
    goto end;
  }

  rv = adm_cluster_insert_disk(disk);
  /* FIXME: Some error checking would be nice. */
  EXA_ASSERT(rv == EXA_SUCCESS);

  rv = conf_save_synchronous();
  /* FIXME: Some error handling would be nice. */
  EXA_ASSERT(rv == EXA_SUCCESS);

  admwrk_barrier(thr_nb, rv, "Save the configuration");

  rv = adm_service_rdev.diskadd(thr_nb, node, disk, request->path);

  rv = admwrk_barrier(thr_nb, rv, "Start the disk");
  if (rv != EXA_SUCCESS)
    goto end;

  /* Trigger a recovery, so that the NBD exports/imports the new disk
     as appropriate. */
  if (adm_is_leader())
    evmgr_request_recovery(adm_wt_get_localmb());

  i = 0;
  while (!disk->imported)
  {

    os_sleep(1);
    if (i++ >= 15)
    {
      exalog_warning("Timeout when waiting for disk %s:%s to be UP",
		     node->name, disk->path);
      rv = -ADMIND_WARN_DISK_RECOVER;
      break;
    }
  }

  exalog_debug("added disk %s:%s ", node->name, disk->path);

end:
  admwrk_ack(thr_nb, rv);
}


const AdmCommand exa_cldiskadd = {
  .code            = EXA_ADM_CLDISKADD,
  .msg             = "cldiskadd",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_cldiskadd,
  .local_commands  = {
     { RPC_ADM_CLDISKADD, local_diskadd },
     { RPC_COMMAND_NULL, NULL }
   }
};

