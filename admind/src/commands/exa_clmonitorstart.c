/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <errno.h>

#include "log/include/log.h"

#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/saveconf.h"
#include "os/include/strlcpy.h"
#include "admind/services/monitor/service_monitor.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "monitoring/md_client/include/md_control.h"


extern struct adm_service adm_service_monitor;


__export(EXA_ADM_CLMONITORSTART) struct cl_monitorstart_params
  {
      char snmpd_host[EXA_MAXSIZE_HOSTNAME + 1];
      uint32_t snmpd_port;
  };



static void
cluster_clmonitorstart(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  int ret;
  struct cl_monitorstart_params *params = data;

  exalog_info("received clmonitorstart from '%s'",
	      adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (adm_cluster.goal != ADM_CLUSTER_GOAL_STARTED)
  {
      set_error(err_desc, -EXA_ERR_ADMIND_STOPPED, NULL);
      return;
  }

  ret = admwrk_exec_command(ctx, &adm_service_monitor, RPC_ADM_CLMONITORSTART,
                            params, sizeof(struct cl_monitorstart_params));

  if (ret != EXA_SUCCESS)
  {
      set_error(err_desc, ret, "%s", exa_error_msg(ret));
      return;

  }
  set_success(err_desc);
}



static void
local_clmonitorstart(admwrk_ctx_t *ctx, void *msg)
{
    int ret, barrier_ret;
    struct cl_monitorstart_params *params = msg;

    strlcpy(adm_cluster.monitoring_parameters.snmpd_host,
	    params->snmpd_host, sizeof(adm_cluster.monitoring_parameters.snmpd_host));
    adm_cluster.monitoring_parameters.snmpd_port = params->snmpd_port;
    adm_cluster.monitoring_parameters.started = true;

    ret = conf_save_synchronous();
    barrier_ret = admwrk_barrier(ctx, ret, "Saving configuration");
    if (barrier_ret != EXA_SUCCESS)
    {
	adm_cluster.monitoring_parameters.started = false;
	goto local_exa_clmonitorstart_end;
    }

    ret = md_client_control_start(adm_wt_get_localmb(),
				  adm_my_id,
				  adm_cluster_get_node_by_id(adm_my_id)->name,
				  adm_cluster.monitoring_parameters.snmpd_host,
				  adm_cluster.monitoring_parameters.snmpd_port);

    barrier_ret = admwrk_barrier(ctx, ret, "Starting monitoring");
    if (barrier_ret != EXA_SUCCESS)
    {
	md_client_control_stop(adm_wt_get_localmb());
	adm_cluster.monitoring_parameters.started = false;
	goto local_exa_clmonitorstart_end;
    }

local_exa_clmonitorstart_end:
    admwrk_ack(ctx, barrier_ret);

}



/**
 * Definition of the exa_clmonitorstart command.
 */
const AdmCommand exa_clmonitorstart = {
  .code            = EXA_ADM_CLMONITORSTART,
  .msg             = "clmonitorstart",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_clmonitorstart,
  .local_commands  = {
    { RPC_ADM_CLMONITORSTART, local_clmonitorstart },
    { RPC_COMMAND_NULL, NULL }
  }
};


