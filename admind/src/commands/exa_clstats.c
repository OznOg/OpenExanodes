/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <string.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "vrt/virtualiseur/include/vrt_client.h"

__export(EXA_ADM_CLSTATS) struct clstats_params
  {
    bool reset;
  };

static void
add_nbd_stats(admwrk_ctx_t *ctx, const struct nbd_stats_request *request,
	      struct nbd_stats_reply *stats)
{
  char buf[1024];
  uint64_t msec;

  msec = stats->now - stats->last_reset;
  os_snprintf(buf, sizeof(buf),
           "<ndev node=\"%s\" disk=\"%s\" msec=\"%"PRIu64"\" seq_sect_read=\"%"PRIu64"\" "
	   "seq_sect_write=\"%"PRIu64"\" seq_req_read=\"%"PRIu64"\" seq_req_write=\"%"PRIu64"\" "
	   "seq_seeks_read=\"%"PRIu64"\" seq_seeks_write=\"%"PRIu64"\" seq_seek_dist_read=\"%"PRIu64"\" "
	   "seq_seek_dist_write=\"%"PRIu64"\" sect_read=\"%"PRIu64"\" sect_write=\"%"PRIu64"\" "
	   "req_read=\"%"PRIu64"\" req_write=\"%"PRIu64"\" req_error=\"%"PRIu64"\" />",
	   request->node_name, request->disk_path, msec, stats->begin.nb_sect_read,
	   stats->begin.nb_sect_write, stats->begin.nb_req_read, stats->begin.nb_req_write,
	   stats->begin.nb_seeks_read, stats->begin.nb_seeks_write, stats->begin.nb_seek_dist_read,
	   stats->begin.nb_seek_dist_write, stats->done.nb_sect_read, stats->done.nb_sect_write,
	   stats->done.nb_req_read, stats->done.nb_req_write, stats->done.nb_req_err);

  send_payload_str(buf);
}


static void
get_nbd_stats(admwrk_ctx_t *ctx, bool reset)
{
  struct adm_node *node;
  struct adm_disk *disk;

  adm_cluster_for_each_node(node)
  {
    adm_node_for_each_disk(node, disk)
    {
      exa_nodeid_t nodeid;
      struct nbd_stats_reply reply_nbd;
      struct nbd_stats_request nbdreq;
      int errval;

      nbdreq.reset = reset;
      strlcpy(nbdreq.node_name, node->name, sizeof(nbdreq.node_name));
      strlcpy(nbdreq.disk_path, disk->path, sizeof(nbdreq.disk_path));
      uuid_copy(&nbdreq.device_uuid, &disk->uuid);

      admwrk_run_command(ctx, &adm_service_nbd,
			 RPC_SERVICE_ADMIND_GETNBDSTATS,
			 &nbdreq, sizeof(nbdreq));

      while (admwrk_get_reply(ctx, &nodeid, &reply_nbd, sizeof(reply_nbd),
			      &errval))
	{
	  char nodetag[128 /* large enougth for tags */ + EXA_MAXSIZE_NODENAME];

	  os_snprintf(nodetag, sizeof(nodetag), "<node name=\"%s\"%s><nbd>",
	           adm_cluster_get_node_by_id(nodeid)->name,
		   errval == -ADMIND_ERR_NODE_DOWN ? " down=\"\"" : "");

	  send_payload_str(nodetag);

	  if (errval != -ADMIND_ERR_NODE_DOWN)
	    add_nbd_stats(ctx, &nbdreq, &reply_nbd);

	  send_payload_str("</nbd></node>");
	}
    }
  }
}


static void
add_vrt_stats(admwrk_ctx_t *ctx, struct vrt_stats_request *request,
	      struct vrt_stats_reply *stats)
{
  char buf[1024];
  uint64_t msec;

  msec = stats->now - stats->last_reset;
  os_snprintf(buf, sizeof(buf),
           "<volume name=\"%s\" msec=\"%"PRIu64"\" seq_sect_read=\"%"PRIu64"\" "
	   "seq_sect_write=\"%"PRIu64"\" seq_req_read=\"%"PRIu64"\" seq_req_write=\"%"PRIu64"\" "
	   "seq_seeks_read=\"%"PRIu64"\" seq_seeks_write=\"%"PRIu64"\" seq_seek_dist_read=\"%"PRIu64"\" "
	   "seq_seek_dist_write=\"%"PRIu64"\" sect_read=\"%"PRIu64"\" sect_write=\"%"PRIu64"\" "
	   "req_read=\"%"PRIu64"\" req_write=\"%"PRIu64"\" req_error=\"%"PRIu64"\" />",
	   request->volume_name, msec, stats->begin.nb_sect_read,
	   stats->begin.nb_sect_write, stats->begin.nb_req_read, stats->begin.nb_req_write,
	   stats->begin.nb_seeks_read, stats->begin.nb_seeks_write, stats->begin.nb_seek_dist_read,
	   stats->begin.nb_seek_dist_write, stats->done.nb_sect_read, stats->done.nb_sect_write,
	   stats->done.nb_req_read, stats->done.nb_req_write, stats->done.nb_req_err);

  send_payload_str(buf);
}


static void
get_vrt_stats(admwrk_ctx_t *ctx, bool reset)
{
  struct adm_group *group;
  struct adm_volume *volume;

  adm_group_for_each_group(group)
  {
    adm_group_for_each_volume(group, volume)
    {
      exa_nodeid_t nodeid;
      struct vrt_stats_request request;
      struct vrt_stats_reply reply_vrt;
      int errval;

      request.reset = reset;
      uuid_copy(&request.group_uuid, &group->uuid);
      strlcpy(request.volume_name, volume->name, sizeof(request.volume_name));

      admwrk_run_command(ctx, &adm_service_vrt,
			 RPC_SERVICE_ADMIND_GETVRTSTATS,
			 &request, sizeof(request));

      while (admwrk_get_reply(ctx, &nodeid, &reply_vrt, sizeof(reply_vrt),
			      &errval))
	{
	  char nodetag[128 /*large enougth for tags bellow */
	               + EXA_MAXSIZE_NODENAME
	               + EXA_MAXSIZE_GROUPNAME];

	  os_snprintf(nodetag, sizeof(nodetag),
	           "<node name=\"%s\"%s><vrt><diskgroup name=\"%s\">",
	           adm_cluster_get_node_by_id(nodeid)->name,
		   errval == -ADMIND_ERR_NODE_DOWN ? " down=\"\"" : "",
		   group->name);
	  send_payload_str(nodetag);

	  if (errval != -ADMIND_ERR_NODE_DOWN)
	    add_vrt_stats(ctx, &request, &reply_vrt);

	  send_payload_str("</diskgroup></vrt></node>");
	}
    }
  }
}


static void
cluster_clstats(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct clstats_params *params = data;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  send_payload_str("<?xml version=\"1.0\"?><stats>");

  get_nbd_stats(ctx, params->reset);
  get_vrt_stats(ctx, params->reset);

  send_payload_str("</stats>");

  set_success(err_desc);
}


static void
local_getnbdstats(admwrk_ctx_t *ctx, void *msg)
{
  const struct nbd_stats_request *request = msg;
  struct nbd_stats_reply reply_msg;

  clientd_stat_get(adm_wt_get_localmb(), request, &reply_msg);

  COMPILE_TIME_ASSERT(sizeof(reply_msg) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(ctx, &reply_msg, sizeof(reply_msg));
}


static void
local_getvrtstats(admwrk_ctx_t *ctx, void *msg)
{
  struct vrt_stats_request *request = msg;
  struct vrt_stats_reply reply_msg;

  vrt_client_stat_get(adm_wt_get_localmb(), request, &reply_msg);

  COMPILE_TIME_ASSERT(sizeof(reply_msg) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(ctx, &reply_msg, sizeof(reply_msg));
}


const AdmCommand exa_clstats = {
  .code            = EXA_ADM_CLSTATS,
  .msg             = "clstats",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_clstats,
  .local_commands  = {
    { RPC_SERVICE_ADMIND_GETNBDSTATS, local_getnbdstats },
    { RPC_SERVICE_ADMIND_GETVRTSTATS, local_getvrtstats },
    { RPC_COMMAND_NULL, NULL }
  }
};

