/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/adm_license.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_config.h"
#include "os/include/os_disk.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "examsgd/examsgd_client.h"


/*
 * FIXME: This should use a single command and barriers instead of the
 *        multiple commands here. This would also move the following
 *        variable to the stack of the local command, and would be
 *        required to support multiple commands executing
 *        simultaneously.
 */
static struct adm_node *new_node;

__export(EXA_ADM_CLNODEADD) struct clnodeadd_params
{
  xmlDocPtr tree;
};

struct msg_nodeadd_begin
{
  exa_nodeid_t id;
  char name[EXA_MAXSIZE_NODENAME + 1];
  char hostname[EXA_MAXSIZE_HOSTNAME + 1];
};


struct msg_nodeadd_network
{
  char hostname[EXA_MAXSIZE_HOSTNAME + 1];
};


struct msg_nodeadd_disk
{
  char path[EXA_MAXSIZE_DEVPATH + 1];
  exa_uuid_t uuid;
};


/* Now, here, picture a C++ templated function, or std::strings. */
#define GET_PROP(dst, node, propname) get_prop(dst, node, propname, \
					       sizeof(dst))
static bool
get_prop(char *dst, xmlNodePtr node, const char *propname, size_t len)
{
  xmlChar *tmpstr;

  tmpstr = xmlGetProp(node, BAD_CAST(propname));
  if (tmpstr != NULL)
    {
      strlcpy(dst, (char*)tmpstr, len);
      xmlFree(tmpstr);
    }

  return tmpstr != NULL;
}

static int
nodeadd_begin(int thr_nb, xmlNodePtr node, cl_error_desc_t *err_desc)
{
  struct msg_nodeadd_begin msg_begin;

  memset(&msg_begin, 0, sizeof(msg_begin));

  EXA_ASSERT(GET_PROP(msg_begin.name, node, "name"));
  EXA_ASSERT(GET_PROP(msg_begin.hostname, node, "hostname"));

  for (msg_begin.id = 0; msg_begin.id < EXA_MAX_NODES_NUMBER; ++msg_begin.id)
    if (adm_cluster.nodes[msg_begin.id] == NULL)
      break;

  if (msg_begin.id == EXA_MAX_NODES_NUMBER)
    {
      set_error(err_desc, -EXA_ERR_DEFAULT,
		"You already have the maximum number of nodes");
      return -EXA_ERR_DEFAULT;
    }

  admwrk_exec_command(thr_nb, &adm_service_admin,
		      RPC_ADM_CLNODEADD_BEGIN,
		      &msg_begin, sizeof(msg_begin));

  return EXA_SUCCESS;
}


static void
nodeadd_network(int thr_nb, xmlNodePtr node)
{
  struct msg_nodeadd_network msg_network;

  memset(&msg_network, 0, sizeof(msg_network));

  GET_PROP(msg_network.hostname, node, "hostname");

  admwrk_exec_command(thr_nb, &adm_service_admin,
		      RPC_ADM_CLNODEADD_NETWORK,
		      &msg_network, sizeof(msg_network));
}


static void
nodeadd_disk(int thr_nb, xmlNodePtr node)
{
  struct msg_nodeadd_disk msg_disk;

  memset(&msg_disk, 0, sizeof(msg_disk));

  os_disk_normalize_path((const char *)xmlGetProp(node, BAD_CAST("path")),
                         msg_disk.path, sizeof(msg_disk.path));
  uuid_scan(xml_get_prop(node, "uuid"), &msg_disk.uuid);

  admwrk_exec_command(thr_nb, &adm_service_admin,
		      RPC_ADM_CLNODEADD_DISK,
		      &msg_disk, sizeof(msg_disk));
}


static int nodeadd_commit(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_admin,
		      RPC_ADM_CLNODEADD_COMMIT,
		      NULL, 0);
}



static void
cluster_clnodeadd(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  struct clnodeadd_params *params = data;
  xmlNodePtr node = xmlDocGetRootElement(params->tree);
  struct adm_node *it;
  const char *nodename;
  const char *hostname;
  xmlNodeSetPtr nodeset;
  unsigned int nb_new_disks;
  exa_nodeset_t all_nodes;
  exa_nodeset_t nodes_up;
  int ret;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  nodename = xml_get_prop(node, "name");
  hostname = xml_get_prop(node, "hostname");
  if (nodename == NULL || hostname == NULL)
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, NULL);
      goto free;
    }

  /* Log this command */
  exalog_info("received clnodeadd '%s' from %s", hostname, adm_cli_ip());

  /* Check all nodes are up to prevent config file mess (see bug #3392) */
  adm_nodeset_set_all(&all_nodes);
  inst_get_nodes_up(&adm_service_admin, &nodes_up);
  if (!exa_nodeset_equals(&all_nodes, &nodes_up))
  {
    set_error(err_desc, -ADMIND_ERR_NODE_DOWN,
              "exa_clnodeadd is not allowed with some nodes down.");
    goto free;
  }

  /* Check that the node to add is not already a member of the
   * cluster.
   */
  adm_cluster_for_each_node(it)
    {
      if (strncmp(it->name, nodename, sizeof(it->name)) == 0 ||
	  strncmp(it->hostname, hostname, sizeof(it->hostname)) == 0)
	  {
	    set_error(err_desc, -EXA_ERR_INVALID_PARAM,
			"The node is already part of the cluster configuration");
	    goto free;
	  }
    }

  if (exa_nodeset_count(evmgr_mship()) <= (adm_cluster_nb_nodes() + 1) / 2)
    {
      set_error(err_desc, -ADMIND_ERR_QUORUM_PRESERVE,
		"Adding a new node would lose the quorum");
      goto free;
    }

    /*
     * Check if the number of nodes is allowed or not
     */
    if (!adm_license_nb_nodes_ok(exanodes_license, adm_cluster_nb_nodes() + 1,
                                 err_desc))
    {
        err_desc->code = -ADMIND_ERR_LICENSE;
        goto free;
    }

  /*
   * The two checks below are a workaround. They are duplicate, too early and
   * not atomic with the insert. They should be removed and local_addnode_disk()
   * should handle errors instead of asserting.
   */

  nodeset = xml_conf_xpath_query(params->tree, "//disk");
  nb_new_disks = xml_conf_xpath_result_entity_count(nodeset);
  xmlXPathFreeNodeSet(nodeset);

  if (adm_cluster_nb_disks() + nb_new_disks > NBMAX_DISKS)
  {
    set_error(err_desc, -ADMIND_ERR_TOO_MANY_DISKS,
              "Too many disks in the cluster (max %u)", NBMAX_DISKS);
    goto free;
  }

  if (nb_new_disks > NBMAX_DISKS_PER_NODE)
  {
    set_error(err_desc, -ADMIND_ERR_TOO_MANY_DISKS_IN_NODE,
              "Too many disks in node '%s' (max %u)",
              NBMAX_DISKS_PER_NODE, nodename);
    goto free;
  }

  if (nodeadd_begin(thr_nb, node, err_desc) != EXA_SUCCESS)
    goto free;

  for (node = node->children; node != NULL; node = node->next)
    {
      if (xmlStrEqual(node->name, BAD_CAST("network")))
	  nodeadd_network(thr_nb, node);

      if (xmlStrEqual(node->name, BAD_CAST("disk")))
	  nodeadd_disk(thr_nb, node);
    }

  ret = nodeadd_commit(thr_nb);

  /*
   * Request a recovery, to verify that we still have the quorum and
   * that everything is okay. It should not actually do the recovery,
   * because nothing needs it (and if something needed it, well, there
   * it goes, it got it).
   */
  evmgr_request_recovery(adm_wt_get_localmb());

  set_error(err_desc, ret, NULL);
free:
  xmlFreeDoc(params->tree);
}


static void
local_addnode_begin(int thr_nb, void *msg)
{
  struct msg_nodeadd_begin *request = msg;

  EXA_ASSERT(msg != NULL);

  if (new_node != NULL)
    adm_node_delete(new_node);

  new_node = adm_node_alloc();
  EXA_ASSERT(new_node != NULL);

  new_node->id = request->id;
  strlcpy(new_node->name, request->name, sizeof(new_node->name));
  strlcpy(new_node->hostname, request->hostname, sizeof(new_node->hostname));

  adm_node_set_spof_id(new_node,
          adm_node_get_first_free_spof_id());

  exalog_debug("adding %s as node %s (id %i)",
	       new_node->hostname, new_node->name, new_node->id);

  admwrk_ack(thr_nb, EXA_SUCCESS);
}


static void
local_addnode_network(int thr_nb, void *msg)
{
  struct msg_nodeadd_network *request = msg;
  struct adm_nic *nic = NULL;
  int rv;

  rv = adm_nic_new(request->hostname, &nic);
  if (rv == EXA_SUCCESS)
      rv = adm_node_insert_nic(new_node, nic);

  /* in case of error free is done by node_free at top level */
  if (rv != EXA_SUCCESS)
      exalog_error("Failed to add nic with hostname='%s' to node '%s': %s (%d)",
	      request->hostname, new_node->name,
	      exa_error_msg(rv), rv);

  admwrk_ack(thr_nb, rv);
}


static void
local_addnode_disk(int thr_nb, void *msg)
{
  struct msg_nodeadd_disk *request = msg;
  struct adm_disk *disk;
  int rv;

  disk = adm_disk_alloc();
  EXA_ASSERT(disk != NULL);

  strlcpy(disk->path, request->path, sizeof(disk->path));
  disk->uuid = request->uuid;
  disk->node_id = new_node->id;

  rv = adm_node_insert_disk(new_node, disk);
  /* FIXME: Some error handling would be nice. */
  EXA_ASSERT(rv == EXA_SUCCESS);

  exalog_debug("adding disk %s to node %s: %s", disk->path, new_node->name,
               exa_error_msg(rv));

  admwrk_ack(thr_nb, rv);
}


static void
cleanup_new_node(int thr_nb, int rv, struct adm_node *node)
{
  exalog_warning("addition of node %s aborted", node->name);

  if (new_node != NULL)
    {
      adm_node_delete(new_node);
      new_node = NULL;
    }

  admwrk_ack(thr_nb, rv);
}


static void
local_addnode_commit(int thr_nb, void *msg)
{
  const struct adm_service *service;
  struct adm_nic *nic;
  struct adm_node *node = new_node;
  int rv;

  nic = adm_node_get_nic(node);

  if (nic == NULL)
    {
      cleanup_new_node(thr_nb, -ADMIND_ERR_UNKNOWN_NICNAME, node);
      return;
    }

  rv = inst_node_add(node);
  /* If this fails, we're in big trouble (out of memory, most likely). */
  EXA_ASSERT(rv == EXA_SUCCESS);

  /* FIXME: I am not entirely sure of the proper order of all this. */

  adm_service_for_each(service)
  {
    if (service->nodeadd == NULL)
      continue;

    rv = service->nodeadd(thr_nb, node);
    if (rv != EXA_SUCCESS)
      {
	exalog_error("%s->nodeadd: %s", adm_service_name(service->id),
		     exa_error_msg(rv));
	cleanup_new_node(thr_nb, rv, node);
	return;
      }
  }

  rv = adm_cluster_insert_node(node);

  if (rv != EXA_SUCCESS)
    {
      exalog_error("adm_node_insert_node: %s", exa_error_msg(rv));
      cleanup_new_node(thr_nb, rv, node);
      return;
    }

  /*
   * From this point on, we are committed (in the regular English
   * sense, not in a transactional sense!) to the node addition, since
   * we called adm_cluster_insert_node.
   */
  new_node = NULL;

  adm_service_for_each(service)
    if (service->nodeadd_commit != NULL)
      service->nodeadd_commit(thr_nb, node);

  rv = examsgAddNode(adm_wt_get_localmb(), node->id, node->name);
  if (rv != EXA_SUCCESS)
    {
      exalog_error("examsgAddNode: %s", exa_error_msg(rv));
      cleanup_new_node(thr_nb, rv, node);
      return;
    }

  rv = conf_save_synchronous();
  /* FIXME: Some error handling would be nice. */
  EXA_ASSERT(rv == EXA_SUCCESS);

  exalog_debug("addition of node %s done", node->name);

  admwrk_ack(thr_nb, EXA_SUCCESS);
}


const AdmCommand exa_clnodeadd = {
  .code            = EXA_ADM_CLNODEADD,
  .msg             = "clnodeadd",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_clnodeadd,
  .local_commands  = {
     { RPC_ADM_CLNODEADD_BEGIN, local_addnode_begin },
     { RPC_ADM_CLNODEADD_NETWORK, local_addnode_network },
     { RPC_ADM_CLNODEADD_DISK, local_addnode_disk },
     { RPC_ADM_CLNODEADD_COMMIT, local_addnode_commit },
     { RPC_COMMAND_NULL, NULL }
   }
};

