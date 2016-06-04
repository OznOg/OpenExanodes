/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifdef WITH_FS

#include <errno.h>

#include "admind/services/fs/generic_fs.h"
#include "admind/services/fs/service_fs.h"
#include "admind/services/fs/type_gfs.h"
#include "admind/src/adm_fs.h"
#include "fs/include/exa_fsd.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"


int cluster_clinfo_filesystem(admwrk_ctx_t *ctx, xmlNodePtr fs_node,
			      fs_data_t* fs, bool get_fs_size)
{
  exa_nodeid_t nodeid;
  struct fs_info_reply reply;
  fs_request_t request;
  exa_nodeset_t status_mounted = EXA_NODESET_EMPTY;
  exa_nodeset_t status_mounted_ro = EXA_NODESET_EMPTY;
  uint64_t size = 0;
  uint64_t used = 0;
  uint64_t available = 0;
  int size_set = false;
  int ret;

  if (fs->transaction)
    {
      strlcpy(request.mountpoint, fs->mountpoint, sizeof(request.mountpoint));
      strlcpy(request.devpath, fs->devpath, sizeof(request.devpath));
      exa_nodeset_reset(&request.nodeliststatfs);
      exalog_debug("RPC_ADM_CLINFO_FS1(%s)", fs_get_name(fs));
      admwrk_run_command(ctx, &adm_service_fs, RPC_ADM_CLINFO_FS,
			 &request, sizeof(request));
      while (admwrk_get_reply(ctx, &nodeid, &reply, sizeof(reply), &ret))
	{
	  if (ret == -ADMIND_ERR_NODE_DOWN)
	    continue;

	  if (reply.mounted)
	    exa_nodeset_add(&status_mounted, nodeid);

	  if (reply.mounted == EXA_FS_MOUNTED_RO)
	    exa_nodeset_add(&status_mounted_ro, nodeid);

	  if (exa_nodeset_count(&request.nodeliststatfs) == 0 && reply.mounted)
	    exa_nodeset_add(&request.nodeliststatfs, nodeid);
	}

      if ((exa_nodeset_count(&request.nodeliststatfs) > 0) && get_fs_size
	  && ( strcmp(fs->fstype, FS_NAME_GFS) || fs_handle_gfs) )
	{
          exalog_debug("RPC_ADM_CLINFO_FS2(%s)",
		       fs_get_name(fs));

	  admwrk_run_command(ctx, &adm_service_fs, RPC_ADM_CLINFO_FS, &request, sizeof(request));

	  while (admwrk_get_reply(ctx, &nodeid, &reply, sizeof(reply), &ret))
	    {
	      if (ret == -ADMIND_ERR_NODE_DOWN)
		continue;

	      if (reply.capa.size > 0)
		{
		  size = reply.capa.size;
		  used = reply.capa.used;
		  available = reply.capa.free;
		  size_set = true;
		}
	    }
	}
    }

  if (!strcmp(fs->fstype, FS_NAME_GFS))
    xml_set_prop_ok(fs_node, EXA_CONF_FS_HANDLE_GFS, fs_handle_gfs);

  ret = xml_set_prop(fs_node, "transaction", fs->transaction ? ADMIND_PROP_COMMITTED : ADMIND_PROP_INPROGRESS);
  ret = ret ? ret : xml_set_prop(fs_node, "type", fs->fstype);
  ret = ret ? ret : xml_set_prop(fs_node, "mountpoint", fs->mountpoint);
  ret = ret ? ret : xml_set_prop(fs_node, "mount_option", fs->mount_option);
  if (size_set)
    {
      ret = ret ? ret : xml_set_prop_u64(fs_node, "size", size);
      ret = ret ? ret : xml_set_prop_u64(fs_node, "used", used);
      ret = ret ? ret : xml_set_prop_u64(fs_node, "available", available);
    }
  ret = ret ? ret : xml_set_prop_nodeset(fs_node, "goal_started", &fs->goal_started);
  ret = ret ? ret : xml_set_prop_nodeset(fs_node, "goal_started_ro", &fs->goal_started_ro);
  ret = ret ? ret : xml_set_prop_nodeset(fs_node, "status_mounted", &status_mounted);
  ret = ret ? ret : xml_set_prop_nodeset(fs_node, "status_mounted_ro", &status_mounted_ro);

  return ret;
}

void local_clinfo_fs(admwrk_ctx_t *ctx, void *msg)
{
  struct fs_info_reply reply;
  fs_request_t *request;
  int ret;

  request = msg;

  /* Ask the fsd about the fs */

  ret = fsd_is_fs_mounted(adm_wt_get_localmb(), request->devpath);

  reply.mounted = ret >= 0 ? ret : 0;

  /* Get the capacities in bytes */

  if (ret != -ADMIND_ERR_NODE_DOWN && reply.mounted &&
      adm_nodeset_contains_me(&request->nodeliststatfs))
  {
    ret = fsd_df(adm_wt_get_localmb(), request->mountpoint, &reply.capa);
    if (ret != EXA_SUCCESS)
    {
      reply.capa.size = -1;
      reply.capa.used = -1;
      reply.capa.free = -1;
    }
  }
  else
  {
    reply.capa.size = 0;
    reply.capa.used = 0;
    reply.capa.free = 0;
  }

  COMPILE_TIME_ASSERT(sizeof(reply) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(ctx, &reply, sizeof(reply));
}

#endif
