/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>

#include "admind/src/adm_cluster.h"
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
#include "admind/src/commands/exa_clinfo_export.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#ifdef WITH_FS
#include "admind/src/commands/exa_clinfo_filesystem.h"
#include "admind/services/fs/generic_fs.h"
#endif

struct volume_reply
{
  int ret;
  int pad;
  struct vrt_volume_info info;
};

typedef struct {
  exa_uuid_t group;
  exa_uuid_t volume;
} volume_request_t;


void local_clinfo_volume(admwrk_ctx_t *ctx, void *msg)
{
  struct volume_reply reply;
  volume_request_t *request;

  request = msg;

  reply.ret = vrt_client_volume_info(adm_wt_get_localmb(),
				     &request->group, &request->volume,
				     &reply.info);

  COMPILE_TIME_ASSERT(sizeof(reply) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(ctx, &reply, sizeof(reply));
}


int cluster_clinfo_volumes(admwrk_ctx_t *ctx, xmlNodePtr group_node,
			   struct adm_group *group,
			   bool get_fs_info, bool get_fs_size)
{
  xmlNodePtr logical_node;
  struct adm_volume *volume;
  int ret;

  /* Create the logical node in the XML doc */

  logical_node = xmlNewChild(group_node, NULL, BAD_CAST("logical"), NULL);
  if (logical_node == NULL)
  {
    exalog_error("xmlNewChild() returned NULL");
    return -EXA_ERR_XML_ADD;
  }

  adm_group_for_each_volume(group, volume)
  {
    volume_request_t request;
    struct volume_reply reply;
    exa_nodeset_t status_stopped = EXA_NODESET_EMPTY;
    exa_nodeset_t status_started = EXA_NODESET_EMPTY;
    uint64_t size = 0;
    xmlNodePtr volume_node;

    adm_nodeset_set_all(&status_stopped);

    if (volume->committed)
    {
      exa_nodeid_t nodeid;
      uuid_copy(&request.group, &group->uuid);
      uuid_copy(&request.volume, &volume->uuid);

      exalog_debug("RPC_ADM_CLINFO_VOLUME(%s)", volume->name);

      admwrk_run_command(ctx, &adm_service_admin, RPC_ADM_CLINFO_VOLUME, &request, sizeof(request));

      while (admwrk_get_reply(ctx, &nodeid, &reply, sizeof(reply), &ret))
        {
          if (ret == -ADMIND_ERR_NODE_DOWN)
            continue;

          EXA_ASSERT(ret == EXA_SUCCESS);

          if (reply.ret == -ENOENT)
            continue;

          EXA_ASSERT(reply.ret == EXA_SUCCESS);
          EXA_ASSERT(strlen(reply.info.name) > 0);

          switch(reply.info.status)
            {
            case EXA_VOLUME_STOPPED:
              break;

            case EXA_VOLUME_STARTED:
              exa_nodeset_del(&status_stopped, nodeid);
              exa_nodeset_add(&status_started, nodeid);
              break;

            default:
                EXA_ASSERT_VERBOSE(false, "Invalid volume status: %d",
                                   reply.info.status);
              break;
            }

          size = reply.info.size;
        }
    }

    /* Create the volume node in the XML doc */
    volume_node = xmlNewChild(logical_node, NULL, BAD_CAST("volume"), NULL);
    if (volume_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      return -EXA_ERR_XML_ADD;
    }

    ret =             xml_set_prop(volume_node, "name", volume->name);
    ret = ret ? ret : xml_set_prop(volume_node, "accessmode", volume->shared ? ADMIND_PROP_SHARED : ADMIND_PROP_PRIVATE);
    ret = ret ? ret : xml_set_prop(volume_node, "transaction", volume->committed ? ADMIND_PROP_COMMITTED : ADMIND_PROP_INPROGRESS);
    if (group->started)
        ret = ret ? ret : xml_set_prop_u64(volume_node, "size", size * 1024);

    ret = ret ? ret : xml_set_prop_nodeset(volume_node, "goal_started", &volume->goal_started);
    ret = ret ? ret : xml_set_prop_nodeset(volume_node, "goal_stopped", &volume->goal_stopped);
    ret = ret ? ret : xml_set_prop_nodeset(volume_node, "goal_readonly", &volume->goal_readonly);
    ret = ret ? ret : xml_set_prop_nodeset(volume_node, "status_stopped", &status_stopped);
    ret = ret ? ret : xml_set_prop_nodeset(volume_node, "status_started", &status_started);

    if (ret != EXA_SUCCESS)
      return ret;

    /* Generate the 'export' children of the 'volume' XML node */
    ret = cluster_clinfo_export_by_volume(ctx, volume_node, &volume->uuid);
    if (ret != EXA_SUCCESS)
	return ret;

#ifdef WITH_FS
    /* Is this a File System ? Do we want any info on this ? */
    if (volume->filesystem && get_fs_info)
      {
	/* Yes. We will add relevant information */
	int ret;
	fs_data_t fs_data;
	xmlNodePtr fs_node;
	fs_node = xmlNewChild(volume_node, NULL, BAD_CAST("fs"), NULL);
	if (fs_node == NULL)
	  {
	    exalog_error("xmlNewChild() returned NULL");
	    return -EXA_ERR_XML_ADD;
	  }
	fs_fill_data_from_config(&fs_data, volume->filesystem);
	ret = cluster_clinfo_filesystem(ctx, fs_node, &fs_data, get_fs_size);
	if (ret)
	  return ret;
      }
#endif
  }

  return EXA_SUCCESS;
}

