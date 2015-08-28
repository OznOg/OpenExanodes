/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "errno.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admind/services/csupd.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "os/include/os_file.h"
#include "os/include/os_time.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"

__export(EXA_ADM_CLSHUTDOWN) struct clshutdown_params
  {
    char node_names[EXA_MAXSIZE_HOSTSLIST + 1];
  };

/* Check if the given set of nodes is really seen as down */
static int
nodes_are_stopped(const exa_nodeset_t *nodeset)
{
  exa_nodeid_t nodeid;
  struct adm_node *node;

  exa_nodeset_foreach(nodeset, nodeid)
  {
    node = adm_cluster_get_node_by_id(nodeid);
    EXA_ASSERT(node != NULL);
    if (!inst_is_node_stopped(node))
      return false;
  }

  return true;
}


/*---------------------------------------------------------------------------*/
/** \brief Implements the clshutdown command
 *
 * \param our node name
 * \param the configuration file
 * - Initialise the examsg communication chanel
 */
/*---------------------------------------------------------------------------*/
static void
cluster_clshutdown(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct clshutdown_params *params = data;
  int error_val;
  ExamsgAny msg;
  int s;
  int ack;
  exa_nodeset_t nodeset;

  /* We don't check the license status to send warnings/errors in this
   * command, because its status has already been checked by clnodestop */

  /* An empty string means "all nodes" */
  if (params->node_names[0] == '\0')
    adm_nodeset_set_all(&nodeset);
  else if (adm_nodeset_from_names(&nodeset, params->node_names) != EXA_SUCCESS)
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, "Unable to parse node list.");
      return;
    }

  exalog_info("received clshutdown '%s' from %s",
	      params->node_names[0] == 0 ? "all": params->node_names,
	      adm_cli_ip());

  /* On the nodes that are not requested to shut down, wait until the
   * nodes to shut down are seen as down. This is mandatory in order
   * to avoid a race between exa_clnodestop and exa_clnodedel.
   */
  if (!adm_nodeset_contains_me(&nodeset))
  {
    int i = 0;

    exalog_debug("Waiting for nodes " EXA_NODESET_FMT " to be shut down",
		 EXA_NODESET_VAL(&nodeset));

    while (!nodes_are_stopped(&nodeset))
    {
      os_sleep(1);
      if (i++ >= 20)
      {
	exalog_warning("Timeout when waiting for nodes "
		       EXA_NODESET_FMT " to be shut down",
		       EXA_NODESET_VAL(&nodeset));
	break;
      }
    }

    error_val = EXA_SUCCESS;
    goto end;
  }

  /* send event */
  msg.type = EXAMSG_ADM_CLSHUTDOWN;

  exalog_debug("   cmd_clshutdown sending CLSHUTDOWN event to the event manager");

  s = examsgSendWithAck(adm_wt_get_localmb(), EXAMSG_ADMIND_EVMGR_ID,
			EXAMSG_LOCALHOST, (Examsg*)&msg, sizeof(msg), &ack);
  exalog_debug("The event thread has done its hierarchy_run_shutdown. "
	       "is_leader=%d s=%d ack=%d", adm_is_leader(), s, ack);

  EXA_ASSERT(s == sizeof(msg));

  error_val = ack;
  /* The command cannot actually be performed in this state, this is not a
   * fatal error, we just reply to the CLI and ignore the command */
  if (error_val == -EXA_ERR_ADMIND_STARTED || error_val == -ADMIND_ERR_INRECOVERY)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  if (ack != EXA_SUCCESS)
    exalog_error("Event thread failed to exec EXAMSG_ADM_CLSHUTDOWN: %s",
		  exa_error_msg(ack));

end:
  exalog_debug("Done (error=%d)", error_val);
  set_error(err_desc, error_val, NULL);
}


/**
 * Definition of the exa_clshutdown command.
 */
const AdmCommand exa_clshutdown = {
  .code            = EXA_ADM_CLSHUTDOWN,
  .msg             = "clshutdown",
  .accepted_status = ADMIND_STARTING | ADMIND_STARTED,
  .match_cl_uuid   = true,
  .allowed_in_recovery = true,
  .cluster_command = cluster_clshutdown,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};
