/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/csupd.h"

#include "os/include/os_error.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_monitor.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_incarnation.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "examsgd/examsgd_client.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"
#include "os/include/os_daemon_parent.h"

static os_daemon_t csupd_daemon;
static os_daemon_t examsgd_daemon;

int
csupd_init(void)
{
  char admind_pid_str[8], node_id_str[8], inca_str[16];
  char csupd_path[OS_PATH_MAX];
  char *const csupd_argv[] = {
    csupd_path,
    "-i", node_id_str,
    "-I", inca_str,
    "-p", (char *)adm_cluster_get_param_text("heartbeat_period"),
    "-t", (char *)adm_cluster_get_param_text("alive_timeout"),
    "-a", admind_pid_str,
    NULL
  };

  exa_env_make_path(csupd_path, sizeof(csupd_path), exa_env_sbindir(), "exa_csupd");
  os_snprintf(admind_pid_str, sizeof(admind_pid_str),
              "%"PRIu32, os_daemon_current_pid());
  os_snprintf(node_id_str, sizeof(node_id_str), "%d", adm_my_id);
  os_snprintf(inca_str, sizeof(inca_str), "%hu", incarnation);

  /* Launch daemon exa_csupd */
  if (os_daemon_spawn(csupd_argv, &csupd_daemon) != 0)
    return -ADMIN_ERR_CSUPDSTART;

  adm_monitor_register(EXA_DAEMON_CSUPD, csupd_daemon);

  return EXA_SUCCESS;
}

int
examsg_init(void)
{
  char msgd_path[OS_PATH_MAX];
  exa_uuid_str_t cluster_id_str;
  char node_id_str[8], inca_str[16];
  int ret;

  char *const examsgd_argv[] = {
    msgd_path,
    "-m", (char *)adm_cluster_get_param_text("multicast_address"),
    "-p", (char *)adm_cluster_get_param_text("multicast_port"),
    "-c", cluster_id_str,
    "-i", node_id_str,
    "-n", (char *)adm_myself()->name,
    "-N", (char *)adm_myself()->hostname,
    "-I", inca_str,
    NULL
  };

  EXA_ASSERT(adm_myself() != NULL);

  ret = adm_set_incarnation();
  if (ret < 0)
    return ret;

  exa_env_make_path(msgd_path, sizeof(msgd_path), exa_env_sbindir(), "exa_msgd");
  os_snprintf(node_id_str, sizeof(node_id_str), "%d", adm_my_id);
  os_snprintf(inca_str, sizeof(inca_str), "%hu", incarnation);

  uuid2str(&adm_cluster.uuid, cluster_id_str);

  /* Launch daemon exa_msgd */
  ret = os_daemon_spawn(examsgd_argv, &examsgd_daemon);
  if (ret != EXA_SUCCESS)
    {
      exalog_error("Failed to launch messaging daemon exa_msgd '%s' (%d)",
		   exa_error_msg(ret), ret);
      return -ADMIND_ERR_EXAMSGDSTART;
    }

  adm_monitor_register(EXA_DAEMON_MSGD, examsgd_daemon);

  return EXA_SUCCESS;
}

int
csupd_shutdown(void)
{
    /* Stop exa_csupd */
    if (adm_monitor_terminate(EXA_DAEMON_CSUPD) != 0)
	return -ADMIN_ERR_CSUPDSTOP;

    adm_monitor_unregister(EXA_DAEMON_CSUPD);

    /* Now we know for sure we are not the leader */
    adm_leader_id  = EXA_NODEID_NONE;
    adm_leader_set = false;

    return EXA_SUCCESS;
}

int examsg_shutdown(ExamsgHandle mh)
{
    adm_monitor_unregister(EXA_DAEMON_MSGD);

    /* Stop exa_msgd */
    examsgdExit(mh);

    os_daemon_wait(examsgd_daemon);

    return EXA_SUCCESS;
}

