/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/services/monitor/service_monitor.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/saveconf.h"
#include "admind/src/rpc.h"
#include "admind/src/adm_monitor.h"
#include "common/include/exa_config.h"
#include "common/include/exa_env.h"
#include "common/include/exa_names.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "admind/src/instance.h"
#include "monitoring/md_client/include/md_control.h"
#include <unistd.h>

#include "os/include/strlcpy.h"
#include "os/include/os_daemon_child.h"

extern exa_nodeid_t adm_my_id;

static os_daemon_t monitor_daemon;

static int
monitor_init(int ctx)
{
  char examd_path[OS_PATH_MAX];
  int ret;

  char *const argv[] = {
      examd_path,
      NULL
  };

  exa_env_make_path(examd_path, sizeof(examd_path), exa_env_sbindir(), "exa_md");

  /* Start the md daemon */
  ret = os_daemon_spawn(argv, &monitor_daemon);

  /* FIXME maybe I could be a better idea to return the daemon error directly.
   * For now I let it like that because I did not check if any caller trap
   * the -MD_ERR_START value... */
  if (ret != 0)
      return -MD_ERR_START;

  adm_monitor_register(EXA_DAEMON_MONITORD, monitor_daemon);

  return ret;
}



static void
monitor_recover_local(admwrk_ctx_t *ctx, void *msg)
{
    int ret = EXA_SUCCESS;
    int barrier_ret;
    exa_nodeset_t nodes_up;

    exalog_debug("monitor recover");
    inst_get_nodes_going_up(&adm_service_monitor, &nodes_up);
    if (adm_nodeset_contains_me(&nodes_up))
    {
	ret = md_client_control_start(adm_wt_get_localmb(),
				      adm_my_id,
				      adm_cluster_get_node_by_id(adm_my_id)->name,
				      adm_cluster.monitoring_parameters.snmpd_host,
				      adm_cluster.monitoring_parameters.snmpd_port);

    }
    barrier_ret = admwrk_barrier(ctx, ret, "Starting monitoring");
    if (barrier_ret != EXA_SUCCESS)
    {
	md_client_control_stop(adm_wt_get_localmb());
	adm_cluster.monitoring_parameters.started = false;
    }
    admwrk_ack(ctx, barrier_ret);
}



static int
monitor_recover(int ctx)
{
    if (adm_cluster.monitoring_parameters.started)
    {
	return admwrk_exec_command(ctx, &adm_service_monitor,
				   RPC_SERVICE_MONITOR_RECOVER,
				   &adm_cluster.monitoring_parameters,
				   sizeof(struct adm_cluster_monitoring));
    }
    return EXA_SUCCESS;
}



static int
monitor_shutdown(int ctx)
{
    /* Stop the md daemon */
    if (adm_monitor_terminate(EXA_DAEMON_MONITORD) != 0)
        return -MD_ERR_STOP;

    adm_monitor_unregister(EXA_DAEMON_MONITORD);

    return EXA_SUCCESS;
}




const struct adm_service adm_service_monitor =
{
  .id = ADM_SERVICE_MONITOR,
  .init = monitor_init,
  .recover = monitor_recover,
  .shutdown = monitor_shutdown,
  .local_commands =
  {
    { RPC_SERVICE_MONITOR_RECOVER,  monitor_recover_local},
    { RPC_COMMAND_NULL, NULL }
  }
};
