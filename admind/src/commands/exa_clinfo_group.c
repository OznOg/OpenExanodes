/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>

#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_control.h"
#endif

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_group.h"
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
#include "admind/src/adm_monitor.h"
#include "admind/include/service_vrt.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "vrt/virtualiseur/include/vrt_client.h"

typedef struct
{
  int ret;
  int pad;
  struct vrt_realdev_rebuild_info info;
} disk_rebuild_info_reply_t;

typedef struct {
  exa_uuid_t group;
  char node_name[EXA_MAXSIZE_NODENAME + 1];
  exa_uuid_t device_uuid;
} disk_rebuild_info_request_t;


/**
 * Tells if the group is administrable or not FROM A LOCAL POINT OF VIEW.
 * A group is known as 'administrable' if we can write the group metadata
 * in the VRT executive.
 *
 * @param[in] group  Pointer to the group structure
 *
 * @return true or false
 */
static bool vrt_group_is_administrable(const struct adm_group *group)
{
    struct adm_disk *disk;

    exa_nodeset_t nodes_with_disks;
    exa_nodeset_t nodes_with_disks_up;

    EXA_ASSERT(group != NULL);

    exa_nodeset_reset(&nodes_with_disks);
    exa_nodeset_reset(&nodes_with_disks_up);

    /* identify the nodes containing some disks from the group and those having
     * at least one disk up in the group
     */
    adm_group_for_each_disk(group, disk)
    {
        exa_nodeset_add(&nodes_with_disks, disk->node_id);

        if (disk->up_in_vrt)
            exa_nodeset_add(&nodes_with_disks_up, disk->node_id);
    }

    /* Be carefull this formula to decide if the group is administrable or not
     * is arbitrary.
     * "Administrable" means that we are able to write the group metadata in the
     * executive and that we will be able to read them eventually.
     *
     * Considering that the group metadata are stored in its disks superblocks,
     * the group is not administrable if there is no disk up in the VRT, and we
     * dont need much more if we consider that it is admind that tells us which
     * version of the superblocks is valid.
     * But using such a weak constraint would allow us to change the superblocks
     * while there are very few disks UP and it would make those few disk a
     * point of failure. Especially if we consider that the group metadata are
     * overwritten at each VRT recovery.
     */

    return exa_nodeset_count(&nodes_with_disks_up) > 0
        && exa_nodeset_count(&nodes_with_disks_up)
                  >= quotient_ceil64(exa_nodeset_count(&nodes_with_disks), 2);
}

void local_clinfo_group_disk(admwrk_ctx_t *ctx, void *msg)
{
  disk_rebuild_info_reply_t reply;
  disk_rebuild_info_request_t *request;

  request = msg;

  reply.ret = vrt_client_rdev_rebuild_info(adm_wt_get_localmb(), &request->group,
                                           &request->device_uuid,
                                           &reply.info);

  COMPILE_TIME_ASSERT(sizeof(reply) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(ctx, &reply, sizeof(reply));
}


int cluster_clinfo_group_disks(admwrk_ctx_t *ctx, xmlNodePtr group_node, struct adm_group *group)
{
  xmlNodePtr physical_node;
  xmlNodePtr disk_node;
  struct adm_disk *disk;
  struct adm_node *node;
  int ret;

  /* Create the physical node in the XML doc */

  physical_node = xmlNewChild(group_node, NULL, BAD_CAST("physical"), NULL);
  if (physical_node == NULL)
  {
    exalog_error("xmlNewChild() returned NULL");
    return -EXA_ERR_XML_ADD;
  }

  adm_group_for_each_disk(group, disk)
  {
    /* Create the device node in the XML doc */

    disk_node = xmlNewChild(physical_node, NULL, BAD_CAST("disk"), NULL);
    if (disk_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      return -EXA_ERR_XML_ADD;
    }

    node = adm_cluster_get_node_by_id(disk->node_id);
    EXA_ASSERT(node != NULL);

    ret =             xml_set_prop(disk_node, "node", node->name);
    ret = ret ? ret : xml_set_prop_uuid(disk_node, "uuid", &disk->uuid);

    if (group->started)
    {
      exa_nodeset_t nodes;
      exa_nodeid_t nodeid;
      disk_rebuild_info_reply_t reply;
      disk_rebuild_info_request_t request;
      uint64_t size_to_rebuild = 0;
      uint64_t rebuilt_size = 0;
      int global_ret= EXA_SUCCESS;
      struct vrt_realdev_info rdev_info;

      /* Get the information on the device */
      ret = vrt_client_rdev_info(adm_wt_get_localmb(), &group->uuid,
				 &disk->vrt_uuid,
				 &rdev_info);
      if (ret != EXA_SUCCESS)
      {
          exalog_error("Info collection on disk "UUID_FMT
                       " of group "UUID_FMT "failed: %s (%d)",
                       UUID_VAL(&group->uuid), UUID_VAL(&disk->vrt_uuid),
                       exa_error_msg(ret), ret);
	return ret;
      }

      ret =             xml_set_prop(disk_node, "status", vrtd_realdev_status_str(rdev_info.status));
      ret = ret ? ret : xml_set_prop_u64(disk_node, "size", rdev_info.size * 1024);
      ret = ret ? ret : xml_set_prop_u64(disk_node, "size_used", rdev_info.capacity_used * 1024);

      /* Get the information on the (possible) rebuilding of the device
       * Rq: we must call directly the instance of VRT in charge of the
       * rebuilding (local to the device) as this information is not distributed
       */
      uuid_copy(&request.group, &group->uuid);
      strlcpy(request.node_name, node->name, sizeof(request.node_name));
      uuid_copy(&request.device_uuid, &disk->vrt_uuid);

      inst_get_current_membership_cmd(&adm_service_vrt, &nodes);

      admwrk_run_command(ctx, &nodes,
                         RPC_ADM_CLINFO_GROUP_DISK, &request, sizeof(request));

      while (!exa_nodeset_is_empty(&nodes))
      {
	  admwrk_get_reply(ctx, &nodes, &nodeid, &reply, sizeof(reply), &ret);
          if (ret == -ADMIND_ERR_NODE_DOWN)
              continue;

          if (ret != EXA_SUCCESS)
          {
              exalog_error("Rebuild info collection for disk "UUID_FMT
                           " of group "UUID_FMT" failed: %s (%d)",
                           UUID_VAL(&group->uuid), UUID_VAL(&disk->vrt_uuid),
                           exa_error_msg(ret), ret);
              global_ret = ret;
              continue;
          }

          if (reply.ret == -VRT_ERR_LAYOUT_UNKNOWN_OPERATION
              || reply.ret == -VRT_ERR_DISK_NOT_LOCAL)
              continue;

          /* This is a workaround for bug #4616 */
          if (reply.ret == -VRT_ERR_UNKNOWN_GROUP_UUID)
          {
              exalog_warning("Node '%s' cannot retrieve rebuilding information"
                             "of group '%s'", node->name, group->name);
              continue;
          }

          if (reply.ret != EXA_SUCCESS)
          {
              exalog_error("Remote rebuild info collection of disk "UUID_FMT
                           " of group "UUID_FMT" failed with error: %s (%d)",
                           UUID_VAL(&disk->vrt_uuid), UUID_VAL(&group->uuid),
                           exa_error_msg(reply.ret), reply.ret);
              global_ret = reply.ret;
              continue;
          }

          size_to_rebuild = reply.info.size_to_rebuild;
          rebuilt_size = reply.info.rebuilt_size;
      }
      if (global_ret != EXA_SUCCESS)
          return global_ret;

      ret = ret ? ret : xml_set_prop_u64(disk_node, "size_to_rebuild", size_to_rebuild * 1024);
      ret = ret ? ret : xml_set_prop_u64(disk_node, "rebuilt_size", rebuilt_size * 1024);

      if (ret != EXA_SUCCESS)
	return ret;
    }
  }

  return ret;
}


int cluster_clinfo_groups(admwrk_ctx_t *ctx, xmlNodePtr exanodes_node,
			  bool get_disks_info, bool get_vl_info,
			  bool get_fs_info, bool get_fs_size)
{
  struct adm_group *group;
  xmlNodePtr group_node;
  int ret = EXA_SUCCESS;

  adm_group_for_each_group(group)
  {
    struct vrt_group_info info;
    const char *status;
    struct adm_disk *disk;
    int nb_disks = 0;
    int info_ret;

    exalog_debug("vrt_client_group_info(%s)", group->name);

    info_ret = vrt_client_group_info(adm_wt_get_localmb(), &group->uuid,
				&info);

    if (info_ret == -ENOENT)
      status = ADMIND_PROP_STOPPED;
    else if (info.status == EXA_GROUP_OK &&
             info.nb_spare != info.nb_spare_available)
      status = "USING SPARE";
    else
      status = vrtd_group_status_str(info.status);

    /* Create the group node in the XML doc */

    group_node = xmlNewChild(exanodes_node, NULL, BAD_CAST("diskgroup"), NULL);
    if (group_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      return -EXA_ERR_XML_ADD;
    }

    /* Create group properties in the XML doc */

    ret =             xml_set_prop(group_node, "transaction", group->committed ? ADMIND_PROP_COMMITTED : ADMIND_PROP_INPROGRESS);
    ret = ret ? ret : xml_set_prop(group_node, "name", group->name);
    ret = ret ? ret : xml_set_prop(group_node, "goal", group->goal == ADM_GROUP_GOAL_STARTED ? ADMIND_PROP_STARTED : ADMIND_PROP_STOPPED);
    ret = ret ? ret : xml_set_prop_u64(group_node, "nb_volumes", group->nb_volumes);
    ret = ret ? ret : xml_set_prop(group_node, "status", status);
    ret = ret ? ret : xml_set_prop(group_node, "tainted", group->tainted ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);
    ret = ret ? ret : xml_set_prop(group_node, "layout", vrt_layout_get_name(group->layout));

    if (info_ret == EXA_SUCCESS)
    {
      ret = ret ? ret : xml_set_prop(group_node, "rebuilding", info.is_rebuilding ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);
      ret = ret ? ret : xml_set_prop_u64(group_node, "size_used", info.used_capacity * 1024);
      ret = ret ? ret : xml_set_prop_u64(group_node, "usable_capacity", info.usable_capacity * 1024);
      ret = ret ? ret : xml_set_prop_u64(group_node, "nb_spare", info.nb_spare);
      ret = ret ? ret : xml_set_prop_u64(group_node, "nb_spare_available", info.nb_spare_available);
      ret = ret ? ret : xml_set_prop_u64(group_node, "slot_width", info.slot_width);
      ret = ret ? ret : xml_set_prop_u64(group_node, "su_size", info.su_size);
      ret = ret ? ret : xml_set_prop_u64(group_node, "chunk_size", info.chunk_size);
      ret = ret ? ret : xml_set_prop(group_node, "dirty_zone_size", info.dirty_zone_size);
      ret = ret ? ret : xml_set_prop(group_node, "blended_stripes", info.blended_stripes);
    }

    if (ret != EXA_SUCCESS)
      return ret;

    /* Count the number of disks */
    adm_group_for_each_disk(group, disk)
      nb_disks++;

    xml_set_prop_u64(group_node, "nb_disks", nb_disks);

    ret = xml_set_prop(group_node, "administrable",
                       vrt_group_is_administrable(group) ? ADMIND_PROP_TRUE :
			                          ADMIND_PROP_FALSE);
    if (ret != EXA_SUCCESS)
      return ret;

    /* Get disks */
    if (get_disks_info)
    {
      ret = cluster_clinfo_group_disks(ctx, group_node, group);
      if (ret != EXA_SUCCESS)
	return ret;
    }

    /* Get volumes */
    if (get_vl_info || get_fs_info)
    {
	ret = cluster_clinfo_volumes(ctx, group_node, group,
				     get_fs_info, get_fs_size);
      if (ret != EXA_SUCCESS)
	return ret;
    }
  }

  return EXA_SUCCESS;
}
