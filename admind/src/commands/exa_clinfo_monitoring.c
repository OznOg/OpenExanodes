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
#include "admind/src/adm_monitor.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"

/** Get monitoring status */
struct monitoring_reply
{
    md_service_status_t status;
};

void local_clinfo_monitoring(admwrk_ctx_t *ctx, void *msg)
{
    struct monitoring_reply reply;
    memset(&reply, 0, sizeof(reply));

    /* ask for agentx status */
    md_client_control_status(adm_wt_get_localmb(), &reply.status);

    admwrk_reply(ctx, &reply, sizeof(reply));
}


int cluster_clinfo_monitoring(admwrk_ctx_t *ctx, xmlNodePtr cluster_node)
{
  struct monitoring_reply reply;
  exa_nodeid_t nodeid;
  int ret;
  xmlNodePtr monitoring_node;
  exa_nodeset_t monitoring_started_nodes;
  exa_nodeset_t monitoring_stopped_nodes;

  /* if monitoring not started at all, no info is returned */
  if (!adm_cluster.monitoring_parameters.started)
      return EXA_SUCCESS;

  inst_get_current_membership(ctx, &adm_service_monitor, &monitoring_started_nodes);
  adm_nodeset_set_all(&monitoring_stopped_nodes);

  monitoring_node = xmlNewChild(cluster_node, NULL, BAD_CAST(EXA_CONF_MONITORING), NULL);
  if (monitoring_node == NULL)
      return -EXA_ERR_XML_ADD;

  ret = xml_set_prop(monitoring_node, EXA_CONF_MONITORING_SNMPD_HOST,
                 adm_cluster.monitoring_parameters.snmpd_host);
  ret = ret ? ret : xml_set_prop_u64(monitoring_node, EXA_CONF_MONITORING_SNMPD_PORT,
				 adm_cluster.monitoring_parameters.snmpd_port);

  exalog_debug("RPC_ADM_CLINFO_MONITORING");
  admwrk_run_command(ctx, &adm_service_monitor, RPC_ADM_CLINFO_MONITORING, NULL, 0);
  while (admwrk_get_reply(ctx, &nodeid, &reply, sizeof(reply), &ret))
  {
      if ((ret == -ADMIND_ERR_NODE_DOWN) || (reply.status == MD_SERVICE_STOPPED))
      {
	  exa_nodeset_del(&monitoring_started_nodes, nodeid);
      }
      else
      {
	  exa_nodeset_del(&monitoring_stopped_nodes, nodeid);
      }
  }
  ret = ret ? ret : xml_set_prop_nodeset(monitoring_node, EXA_CONF_MONITORING_STARTED_ON,
				     &monitoring_started_nodes);
  ret = ret ? ret : xml_set_prop_nodeset(monitoring_node, EXA_CONF_MONITORING_STOPPED_ON,
				     &monitoring_stopped_nodes);
  return ret;
}

#endif
