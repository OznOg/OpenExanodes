/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/exa_clinfo_volume.h"
#include "admind/src/commands/exa_clinfo_node.h"
#include "admind/src/commands/exa_clinfo_group.h"
#include "admind/src/adm_monitor.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/os_string.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "vrt/virtualiseur/include/vrt_client.h"

struct node_disk_reply
{
  uint64_t size;
  uint32_t missing;
  char path[EXA_MAXSIZE_DEVPATH + 1];
};


void local_clinfo_node_disks(int thr_nb, void *msg)
{
  struct node_disk_reply reply[NBMAX_DISKS_PER_NODE];
  struct adm_disk *disk;
  int i = -1;

  memset(reply, 0, sizeof(reply));

  adm_node_for_each_disk(adm_myself(), disk)
  {
    int ret;

    i++; /* begins at 0 */

    EXA_ASSERT(i < NBMAX_DISKS_PER_NODE);

    os_strlcpy(reply[i].path, disk->path, sizeof(reply[i].path));

    if (disk->local->reachable)
    {
      ret = serverd_get_device_size(adm_wt_get_localmb(), &disk->uuid, &reply[i].size);
      if (ret != EXA_SUCCESS)
      {
	exalog_error("failed to get info for disk %s: %s",
		    disk->path, exa_error_msg(ret));
	continue;
      }
    }

    reply[i].missing = !disk->local->reachable;
  }

  COMPILE_TIME_ASSERT(sizeof(struct node_disk_reply) * NBMAX_DISKS <=
                      ADM_MAILBOX_PAYLOAD_PER_NODE * EXA_MAX_NODES_NUMBER);
  admwrk_reply(thr_nb, reply, sizeof(struct node_disk_reply) * adm_node_nb_disks(adm_myself()));
}

void local_clinfo_disk_info(int thr_nb, void *msg)
{
  struct disk_info_reply reply;
  struct disk_info_query *query = msg;
  struct adm_disk *disk;

  memset(&reply, 0, sizeof(reply));

  disk = adm_node_get_disk_by_uuid(adm_myself(), &query->uuid);
  if (disk)
  {
    reply.has_disk = true;
    uuid_copy(&reply.uuid, &disk->uuid);
    serverd_get_device_size(adm_wt_get_localmb(), &disk->uuid, &reply.size);
    os_strlcpy(reply.path, disk->path, sizeof(reply.path));
    os_strlcpy(reply.status, adm_disk_get_status_str(disk), sizeof(reply.status));
  }
  else
    reply.has_disk = false;


  COMPILE_TIME_ASSERT(sizeof(struct disk_info_reply) <=
                      ADM_MAILBOX_PAYLOAD_PER_NODE * EXA_MAX_NODES_NUMBER);
  admwrk_reply(thr_nb, &reply, sizeof(struct disk_info_reply));
}


static int cluster_clinfo_node_disks(int thr_nb, xmlNodePtr cluster_node)
{
  struct node_disk_reply reply[NBMAX_DISKS_PER_NODE];
  exa_nodeid_t nodeid;
  admwrk_request_t rpc;
  int ret = EXA_SUCCESS;
  int err;

  exalog_debug("RPC_ADM_CLINFO_NODE_DISKS");

  admwrk_run_command(thr_nb, &adm_service_rdev, &rpc, RPC_ADM_CLINFO_NODE_DISKS, NULL, 0);

  while (admwrk_get_reply(&rpc, &nodeid, reply, sizeof(reply), &err))
  {
    struct adm_disk *disk;
    xmlNodePtr node_node;
    int i = -1;

    node_node = xml_get_child(cluster_node, "node", "name", adm_cluster_get_node_by_id(nodeid)->name);
    if (node_node == NULL)
    {
      ret = ADMIND_ERR_UNKNOWN_NODENAME;
      goto error;
    }

    /* Disk should be sorted in the same fashion on all the nodes. Actually
       they are not, eg. if a cldiskadd/remove is running at the same time.
       In this case, clinfo result will be briefly wrong. */

    adm_node_for_each_disk(adm_cluster_get_node_by_id(nodeid), disk)
    {
      xmlNodePtr disk_node;
      exa_uuid_str_t uuid_str;
      i++; /* begins at index 0 */

      uuid2str(&disk->uuid, uuid_str);
      disk_node = xml_get_child(node_node, "disk", "uuid", uuid_str);
      if (disk_node == NULL)
      {
	exalog_error("failed to regain disk %s in XML answer", uuid_str);
	ret = -ADMIND_ERR_UNKNOWN_DISK;
	goto error;
      }

      if (err == -ADMIND_ERR_NODE_DOWN)
      {
	ret = xml_set_prop(disk_node, "status", ADMIND_PROP_DOWN);
	if (ret != EXA_SUCCESS)
	  goto error;
	continue;
      }

      ret = xml_set_prop_u64(disk_node, "size", reply[i].size * 1024);
      ret = ret ? ret : xml_set_prop(disk_node, "path", reply[i].path);

      /* FIXME that's a hack to avoid forgetting a remote disk is missing.
       * It's been set in local_clinfo_node_disks by the node in which the disk
       * is.
       */
      if (reply[i].missing && !disk->broken)
        ret = ret ? ret : xml_set_prop(disk_node, "status", ADMIND_PROP_MISSING);
      if (ret != EXA_SUCCESS)
	goto error;
    }
  }

  return EXA_SUCCESS;

error:
  /* Get remaining replies */
  while (admwrk_get_reply(&rpc, &nodeid, &reply, sizeof(reply), &err));
  return ret;
}


int cluster_clinfo_nodes(int thr_nb, xmlNodePtr cluster_node)
{
  struct adm_node *node;
  int ret;

  adm_cluster_for_each_node(node)
  {
    struct adm_disk *disk;
    xmlNodePtr node_node;

    /* Create the node node in the XML doc */

    node_node = xmlNewChild(cluster_node, NULL, BAD_CAST("node"), NULL);
    if (node_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      return -EXA_ERR_XML_ADD;
    }

    /* Create group properties in the XML doc */

    ret =             xml_set_prop(node_node, "name", node->name);
    ret = ret ? ret : xml_set_prop(node_node, "hostname", node->hostname);
    ret = ret ? ret : xml_set_prop_u64(node_node, "spof_id", adm_node_get_spof_id(node));
    if (ret != EXA_SUCCESS)
      return ret;

    adm_node_for_each_disk(node, disk)
    {
      xmlNodePtr disk_node;
      const char *status;

      /* Create the node node in the XML doc */

      disk_node = xmlNewChild(node_node, NULL, BAD_CAST("disk"), NULL);
      if (disk_node == NULL)
      {
	exalog_error("xmlNewChild() returned NULL");
	return -EXA_ERR_XML_ADD;
      }

      /* Get the status
       * For non-local disks, this will be refined later in
       * cluster_clinfo_node_disks because we can't figure
       * out a MISSING status from here (which is ugly).
       */
      status = adm_disk_get_status_str(disk);

      /* Create group properties in the XML doc */

      ret = xml_set_prop_uuid(disk_node, "uuid", &disk->uuid);
      ret = ret ? ret : xml_set_prop(disk_node, "status", status);
      if (ret != EXA_SUCCESS)
	return -EXA_ERR_XML_ADD;
    }
  }

  /* Get disks info */

  ret = cluster_clinfo_node_disks(thr_nb, cluster_node);

  return ret;
}

