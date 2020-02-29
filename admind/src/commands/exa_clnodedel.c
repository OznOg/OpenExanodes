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
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "examsgd/examsgd_client.h"

struct msg_nodedel
{
  exa_nodeid_t id;
};

__export(EXA_ADM_CLNODEDEL) struct clnodedel_params
{
  char nodename[EXA_MAXSIZE_NODENAME + 1];
};

static void
cluster_clnodedel(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const char *nodename = data;
  const struct adm_node *node;
  struct adm_disk *disk;
  struct msg_nodedel msg;
  const struct adm_service *service;

  /* Log this command */
  exalog_info("received clnodedel '%s' from %s",
	      nodename, adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  node = adm_cluster_get_node_by_name(nodename);
  if (node == NULL)
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
		"Could not find the node specified");
      return;
    }

  /* Return the hostname to CLI/GUI to let it clean the config node cache.*/
  send_payload_str(node->hostname);

  if (!inst_is_node_stopped(node))
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
		"The node is currently up (it should be down)");
      return;
    }

  adm_node_for_each_disk(node, disk)
  if (!uuid_is_zero(&disk->group_uuid))
    {
      struct adm_group *group = adm_group_get_group_by_uuid(&disk->group_uuid);
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
		"Disk %s:" UUID_FMT " is currently used by group %s",
		node->name, UUID_VAL(&disk->uuid), group->name);
      return;
    }

  adm_service_for_each(service)
  {
    if (service->check_nodedel)
    {
      int ret = service->check_nodedel(thr_nb, node);
      if (ret != EXA_SUCCESS)
	{
	  set_error(err_desc, ret, "The node cannot be deleted, %s",
		    exa_error_msg(ret));
	  return;
	}
    }
  }

  msg.id = node->id;

  admwrk_exec_command(thr_nb, &adm_service_admin,
		      RPC_ADM_CLNODEDEL,
		      &msg, sizeof(msg));

  set_success(err_desc);
}

static void
local_delnode(int thr_nb, void *msg)
{
  struct msg_nodedel *request = msg;
  const struct adm_node *node;
  struct adm_node *pending_node;
  const struct adm_service *service;
  int rv;

  node = adm_cluster_get_node_by_id(request->id);

  /* FIXME: Here, things could go *boom* in the info or recovery
   * thread. */
  pending_node = adm_cluster_remove_node(node);
  /* End of the *boom* area. */

  adm_service_for_each_reverse(service)
    if (service->nodedel != NULL)
      service->nodedel(thr_nb, pending_node);

  inst_node_del(pending_node);

  rv = examsgDelNode(adm_wt_get_localmb(), pending_node->id);
  EXA_ASSERT(rv == EXA_SUCCESS);

  rv = conf_save_synchronous();
  EXA_ASSERT(rv == EXA_SUCCESS);

  exalog_debug("removal of node %s done", pending_node->name);

  adm_node_delete(pending_node);

  admwrk_ack(thr_nb, EXA_SUCCESS);
}


const AdmCommand exa_clnodedel = {
  .code            = EXA_ADM_CLNODEDEL,
  .msg             = "clnodedel",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_clnodedel,
  .local_commands  = {
     { RPC_ADM_CLNODEDEL, local_delnode },
     { RPC_COMMAND_NULL, NULL }
   }
};

