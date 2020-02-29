/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file
 * \brief Events processing for event manager thread.
 */

#include "admind/src/evmgr/evmgr.h"

#include <signal.h>
#include <string.h>

#include "admind/include/evmgr_pub_events.h"
#include "admind/services/csupd.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_monitor.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "os/include/os_mem.h" /* meminfo */
#include "common/include/exa_assert.h"
#include "csupd/include/exa_csupd.h"
#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_syslog.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

/* How long in seconds we wait for the beginning of hierarchy_run_start
   after a CLI clwaitstart has been issued */
#define TIMEOUT_CLWAITSTART 120

/* --- local types --------------------------------------------------- */

static bool in_recovery = false;

/* A timeout used in case the quorum is not obtained and the CLI
 * requested a clwaitstart. In this case, at timeout end, a TIMEOUT
 * error is returned to cli_end and then the CLI
 *
 * if >= 0 the value is the time left; if < 0 the timer is inactivated
 */
static int cli_end_timeout = -1;

/* uid of the command that triggered the recovery.
 * This is necessary to be able to ack a cli command asynchronously */
static cmd_uid_t pending_cli_cmd_uid = CMD_UID_INVALID;


/* Event manager message handle. */
static ExamsgHandle evmgr_mh;

typedef struct recovery_end
{
    /* FIXME maybe a generation number of the recovery would be a good thing
     * to be able to know we finished the recovery we were expecting */
    cl_error_desc_t error_desc;
} recovery_end_t;

static void evmgr_recovery_end(const cl_error_desc_t *error_desc);
static int  evmgr_process_shutdown(void);
static void evmgr_analyze_event(const Examsg *msg, const ExamsgMID *from);
static void evmgr_node_stopped_analyze_event(const Examsg *msg, const ExamsgMID *from);

#define THIS_NODE_IS_ACTIVE() (adm_get_state() == ADMIND_STARTED \
                               || (adm_get_state() == ADMIND_STARTING \
                                   && adm_cluster.goal == ADM_CLUSTER_GOAL_STARTED))

/**
 * Make the node crash with a message "need reboot"
 * Before crashing, clientd is killed in order to try to flush IO
 * that would be blocked.
 */
void
crash_need_reboot(char *message)
{
  exalog_error("%s", message);
  exalog_error("Exanodes service needs to restart");
#ifdef WITH_FS
  exalog_error("If SFS is used, the node MUST be rebooted");
#endif
  os_syslog(OS_SYSLOG_ERROR, "%s", message);

  /* Hack to give some time to log the last messages */
  os_sleep(1);

  /* Make sure every daemon knows that admind is trying to quit ; Killing
   * clientd is also a hack to return IO errors so the node does not become
   * frozen because of I/O blocked in exanodes */
  adm_monitor_unsafe_terminate_all(ADM_MONITOR_METHOD_BRUTAL);

  exit(1);
}

/** \brief Init the event manager stuff
 *
 */
int
evmgr_init(void)
{
  int retval;

  /* Initialize exalog */
  exalog_as(EXAMSG_ADMIND_EVMGR_ID);

  /* initialize examsg handle */
  evmgr_mh = examsgInit(EXAMSG_ADMIND_EVMGR_ID);

  if (!evmgr_mh)
    return -EINVAL;

  /* create local mailbox, buffer at most 6 messages */
  retval = examsgAddMbox(evmgr_mh, EXAMSG_ADMIND_EVMGR_ID, 6, EXAMSG_MSG_MAX);
  if (retval)
    return retval;

  /* prepare the deamon liveness-check layer*/
  adm_monitor_init();

  return EXA_SUCCESS;
}

void evmgr_cleanup()
{
    examsgDelMbox(evmgr_mh, EXAMSG_ADMIND_EVMGR_ID);
    examsgExit(evmgr_mh);
}

/**
 * Send the ack reply that the CLI is waiting
 *
 * \param[in] result : value of the ack to send
 * \param[in] msg    : the message to ack
 */
static void
evmgr_ack_cli(int result, const Examsg *msg)
{
  /* if no CLI is connected, we can give up */
  if (msg->any.type == EXAMSG_INVALID)
    return;

  examsgAckReply(evmgr_mh, msg, result, EXAMSG_ADMIND_CMD_LOCAL, EXAMSG_LOCALHOST);
}

/*
 * Sends back an answer to the cli for a given command; deserve to replace
 * evmgr_ack_cli */
static void
evmgr_command_end(cmd_uid_t cuid, const cl_error_desc_t *error_desc)
{
  ExamsgAny header;
  command_end_t command_end;

  /* command must be valid */
  if (cuid == CMD_UID_INVALID)
    return;

  header.type = EXAMSG_ADM_CLUSTER_CMD_END;

  command_end.err_desc = *error_desc;
  command_end.cuid     = cuid;

  examsgSendWithHeader(evmgr_mh, EXAMSG_ADMIND_CLISERVER_ID, EXAMSG_LOCALHOST,
      &header, &command_end, sizeof(command_end));
}

/**
 * Returns true iff the local node is the leader or the command can be
 * performed by any node.
 *
 * @param[in] cmd_code  Command code
 *
 * return true or false
 */
static bool
self_can_handle_cmd(adm_command_code_t cmd_code)
{
  /* the leader can always perform a command */
  if (adm_is_leader())
    return true;

  /* some commands are authorized on any node */
  if (cmd_code == EXA_ADM_GETCONFIG
      || cmd_code == EXA_ADM_GETLICENSE
      || cmd_code == EXA_ADM_SETLICENSE
      || cmd_code == EXA_ADM_GETPARAM
      || cmd_code == EXA_ADM_GET_NODEDISKS
      || cmd_code == EXA_ADM_CLCREATE
      || cmd_code == EXA_ADM_GET_CLUSTER_NAME
      || cmd_code == EXA_ADM_CLINIT
      || cmd_code == EXA_ADM_CLNODESTOP
      || cmd_code == EXA_ADM_CLSHUTDOWN
      || cmd_code == EXA_ADM_CLDELETE
      || cmd_code == EXA_ADM_CLTRACE
      || cmd_code == EXA_ADM_CLTUNE)
    return true;

  return false;
}


/**
 * Tells whether the command must be processed even if the license has expired.
 *
 * @param[in] cmd_code  Command code
 *
 * return true if the command must be processed, false otherwise
 */
static bool evmgr_cmd_bypass_license(adm_command_code_t cmd_code)
{
    if (cmd_code == EXA_ADM_CLINIT
	|| cmd_code == EXA_ADM_CLDISKADD
	|| cmd_code == EXA_ADM_CLDISKDEL
	|| cmd_code == EXA_ADM_CLNODEADD
	|| cmd_code == EXA_ADM_CLNODEDEL
	|| cmd_code == EXA_ADM_DGCREATE
	|| cmd_code == EXA_ADM_DGSTART
	|| cmd_code == EXA_ADM_DGDISKRECOVER
	|| cmd_code == EXA_ADM_VLCREATE
	|| cmd_code == EXA_ADM_VLRESIZE
	|| cmd_code == EXA_ADM_VLSTART
#ifdef WITH_FS
	|| cmd_code == EXA_ADM_FSCREATE
	|| cmd_code == EXA_ADM_FSSTART
#endif
	)
	return false;
    else
	return true;
}

/**
 * Return EXA_SUCCESS on success or a negative error code in case of error
 */
static int evmgr_recv_event(ExamsgMID *from, Examsg *msg, size_t msg_size)
{
  int retval;
  /** Timeout when waiting for an event */
  static const struct timeval EVENT_TIMEOUT = { .tv_sec = 1, .tv_usec = 0 };
  static struct timeval timeout;

  timeout = EVENT_TIMEOUT;

  retval = examsgWaitTimeout(evmgr_mh, &timeout);

  /* an error occurred */
  if (retval != 0 && retval != -ETIME)
    return retval;

  /* Whatever woke us up (a normal message arrived or TIMEOUT), we recheck the
   * TM to be as reactive as we can to reconnect. */
  if (evmgr_mship_token_manager_is_set() && THIS_NODE_IS_ACTIVE())
      evmgr_mship_tm_connect();

  if (retval == -ETIME)
    {
      /* FIXME reseting timeout here seem useless... check it.*/
      timeout = EVENT_TIMEOUT;

      adm_monitor_check();

      if (cli_end_timeout >= 0)
	if (cli_end_timeout-- == 0)
	  {
	    cl_error_desc_t err_desc;

            if (evmgr_mship_trying_to_acquire_token())
                set_error(&err_desc, -ADMIND_ERR_QUORUM_TIMEOUT, "%s (%s)",
                          exa_error_msg(-ADMIND_ERR_QUORUM_TIMEOUT),
                          "Token was denied");
            else
                set_error(&err_desc, -ADMIND_ERR_QUORUM_TIMEOUT, NULL);

	    evmgr_command_end(pending_cli_cmd_uid, &err_desc);
	    pending_cli_cmd_uid = CMD_UID_INVALID;
	  }

#ifdef WITH_MEMTRACE
#define MEMINFO_PERIODICITY  10         /* How many pings between each meminfo */
      {
	static int meminfo_count = 0;

	/* periodicaly test memory in order to discover hypothetical
	 * memory corruption */
	meminfo_count++;
	if (meminfo_count == MEMINFO_PERIODICITY)
	  {
	    meminfo_count = 0;
	    os_meminfo("Admind", OS_MEMINFO_SILENT);
	  }
      }
#endif
      return EXA_SUCCESS;
    }

  /* Note that examsgRecv can return 0 in sometimes when it was actually waked
   * up but no message was there...; this is not an error anyway */
  retval = examsgRecv(evmgr_mh, from, msg, msg_size);

  return (retval == msg_size || retval == 0) ? EXA_SUCCESS : retval;
}

/**
 * \brief adm_hierarchy_call_wt
 * Wrapper around make_worker_thread_exec_command to make it synchronous.
 * when leaving this function the command that match 'code' is finished
 * and the result is given in err_desc
 *
 * This function is called in the context of the event manager
 * thread.
 *
 * \param[in] code             The command code of the cluster command to perform
 * \param[in] cuid             The command uid for this command.
 * \param[in] data             Data for the command
 * \param[in] data_size        The size of the data buffer
 * \param[in] msg_processing   Function called to handle messages while waiting for
 *                             the ack, NULL for pure blocking call.
 *
 * \param[out] err_desc        Error descriptor
 *
 * FIXME : This function deserve to disapear: as the answer is asynchronous,
 * it should be received directly by the evmgr... but until the ack is properly
 * handled there, we keep this function...
 */
static void
adm_hierarchy_call_wt(adm_command_code_t code, cmd_uid_t cuid,
                      const void *data, size_t data_size,
                      void (*msg_processing)(const Examsg *msg,
                                             const ExamsgMID *from),
		      cl_error_desc_t *err_desc)
{
  int ret;

  ret = make_worker_thread_exec_command(evmgr_mh, cuid, code, data, data_size);
  if (ret)
  {
    set_error(err_desc, ret, NULL);
    return;
  }

  while (true)
  {
    Examsg msg;
    ExamsgMID from;

    ret = evmgr_recv_event(&from, &msg, sizeof(msg));
    if (ret == 0)
      continue;

    if (ret < 0)
      {
	exalog_debug("error %d while waiting for recovery to finish", ret);
	set_error(err_desc, ret, NULL);
	return;
      }

    /* if we get the answer to the request we sent */
    if (msg.any.type == EXAMSG_ADM_CLUSTER_CMD_END)
      {
	command_end_t *end = (command_end_t *)msg.payload;
	/* The uid must match otherwise this is a response to another command
	 * (actually not the recovery, thus clinfo :) ) */
	if (cuid == end->cuid)
	 {
	   EXA_ASSERT(from.id == EXAMSG_ADMIND_RECOVERY_ID);
	   if (err_desc)
	     *err_desc = end->err_desc;
	   return;
	 }
      }

    /* if we are here, we have a message to handle so there MUST be a msg
     * processing function */
    EXA_ASSERT(msg_processing);

    /* the message received was not the answer we are waiting for, so we pass it
     * to the process msg function */
    msg_processing(&msg, &from);
  }
}

/**
 * Wait, receive messages and handle them.
 *
 * \return EXA_SUCCESS or a negative error code
 */
int
evmgr_handle_msg(void)
{
  Examsg msg;
  ExamsgMID from;
  int retval;

  retval = evmgr_recv_event(&from, &msg, sizeof(msg));

  if (retval <= 0)
    return retval;

  /*
   * We enter the main process event loop only if started or
   * trying to start.
   */
  if (THIS_NODE_IS_ACTIVE())
    evmgr_analyze_event(&msg, &from);
  else
    evmgr_node_stopped_analyze_event(&msg, &from);

  return EXA_SUCCESS;
}

/**
 * Handle event sent by an instance
 *
 * \param[in] inst_event  : event to handle
 */
static void
handle_instance_event(const instance_event_t *inst_event)
{
  const struct adm_service *service = NULL;
  const struct adm_node *node;

  if (inst_event->id == EXAMSG_RDEV_ID)
    service = &adm_service_rdev;
  else if (inst_event->id == EXAMSG_NBD_SERVER_ID)
    service = &adm_service_nbd;
  else if (inst_event->id == EXAMSG_VRT_ID)
    service = &adm_service_vrt;
  else
    EXA_ASSERT_VERBOSE(false, "%s is not supposed to send messages "
		       " of type  SUP_EVENT_SERVICE",
		       examsgIdToName(inst_event->id));

  node = adm_cluster_get_node_by_id(inst_event->node_id);
  EXA_ASSERT(node);

  if (inst_event->state == INSTANCE_DOWN)
    {
      if (node == adm_myself())
	{
	  /* Kill all exanodes daemons. Killing clientd should make exa_bd
	   * return IO errors to unfreeze processes waiting for memory,
	   * expecially admind itself which needs memory to assert (to log
	   * in syslog and to generate a core file). */
	  adm_monitor_unsafe_terminate_all(ADM_MONITOR_METHOD_BRUTAL);

	  /* sleep before asserting in order to flush logs */
	  os_sleep(3);
	  EXA_ASSERT_VERBOSE(false, "%s say it is down",
	      examsgIdToName(inst_event->id));
	}
      else
	exalog_info("node %s signaled its death", node->name);
    }
  else if (inst_event->state == INSTANCE_CHECK_DOWN)
    {
      exalog_info("service %s need a CHECK down", adm_service_name(service->id));
      exalog_debug("Received a CHECK down on %s", node->name);
      inst_evt_check_down(node, service);

      /* The recovery begin when need_recovery==true */
      /* FIXME why the quorated condition not checked ? */
      if (adm_leader_id == adm_my_id && adm_leader_set)
	need_recovery_set(true);

      in_recovery = true;
    }
  else if (inst_event->state == INSTANCE_CHECK_UP)
    {
      exalog_debug("Received a CHECK UP for service %s on %s",
                   adm_service_name(service->id), node->name);
      inst_evt_check_up(node, service);

      /* The recovery begin when need_recovery==true */
      /* FIXME why the quorated condition not checked ? */
      if (adm_leader_id == adm_my_id && adm_leader_set)
	need_recovery_set(true);

      in_recovery = true;
    }
  else
    EXA_ASSERT_VERBOSE(false, "%s sent invalid instance state: %d",
                       examsgIdToName(inst_event->id),
		       inst_event->state);
}

static void
evmgr_handle_cluster_cmd(const command_t *cmd)
{
  cl_error_desc_t error_desc;
  AdmindStatus status;
  int error_val;

  /* XXX The license should be checked here instead of being checked in each
   * and every command.
   *
   * Indeed, if would be feasible provided each command (in their AdmCommand
   * struct) would tell whether it requires a valid license to work.
   *
   * However, this is not possible as of now, since in-progress messages
   * can't be sent from here, only from command code: in-progress messages
   * are directly sent on the command socket instead of being sent back
   * to the CLI through the "evmgr -> CLI server" pipe, contrarily to
   * "regular" messages.
   */

  /* check if command is accepted in the current admind status */
  status = adm_get_state();

  /* Command need the cluster to be identified */
  if (status != ADMIND_NOCONFIG
      && adm_command_find(cmd->code)->match_cl_uuid
      && !uuid_is_equal(&cmd->cluster_uuid, &adm_cluster.uuid))
    {
      set_error(&error_desc, -EXA_ERR_UUID, NULL);
      evmgr_command_end(cmd->uid, &error_desc);
      return;
    }

  if ((adm_command_find(cmd->code)->accepted_status & status) == 0)
    {
      int err = EXA_ERR_DEFAULT; /* Silence the compiler (uninitialized var) */
      switch (status)
        {
	  case ADMIND_NOCONFIG:
	    err = -EXA_ERR_ADMIND_NOCONFIG;
	    break;

	  case ADMIND_STOPPED:
	    err = -EXA_ERR_ADMIND_STOPPED;
	    break;

	  case ADMIND_STARTING:
	    if (adm_cluster.goal == ADM_CLUSTER_GOAL_STOPPED)
	      err = -EXA_ERR_ADMIND_STOPPING;
	    else
	      err = -EXA_ERR_ADMIND_STARTING;
	    break;

	  case ADMIND_STARTED:
	    err = -EXA_ERR_ADMIND_STARTED;
	    break;
	}
      set_error(&error_desc, err, NULL);
      evmgr_command_end(cmd->uid, &error_desc);
      return;
    }

  /* Some command need to have a not expired license */
  if (!evmgr_cmd_bypass_license(cmd->code))
  {
      adm_license_status_t license_status = adm_license_get_status(exanodes_license);

      if (license_status == ADM_LICENSE_STATUS_EXPIRED)
      {
	  exalog_info("Command aborted due to expired license");
	  set_error(&error_desc, -ADMIND_ERR_LICENSE, "Exanodes' license is expired");
	  evmgr_command_end(cmd->uid, &error_desc);
	  return;
      }
      else if (license_status == ADM_LICENSE_STATUS_NONE)
      {
	  exalog_info("Command aborted due to missing license");
	  set_error(&error_desc, -ADMIND_ERR_LICENSE, "Exanodes has got no license");
	  evmgr_command_end(cmd->uid, &error_desc);
	  return;
      }
  }

  if (!self_can_handle_cmd(cmd->code))
  {
    set_error(&error_desc, -ADMIND_ERR_NOTLEADER, NULL);
    evmgr_command_end(cmd->uid, &error_desc);
    return;
  }

  switch (cmd->code)
  {
    case EXA_ADM_CLINIT:
      evmgr_process_init(cmd->uid, &error_desc);
      if (error_desc.code != EXA_SUCCESS)
      {
	evmgr_command_end(cmd->uid, &error_desc);
	return;
      }

      pending_cli_cmd_uid = cmd->uid;
      break;

    case EXA_ADM_CLNODESTOP:
      if (adm_get_state() == ADMIND_STARTED && !adm_is_leader())
      {
	  set_error(&error_desc, -ADMIND_ERR_NOTLEADER, NULL);
	  evmgr_command_end(cmd->uid, &error_desc);
	  return;
      }

      if (in_recovery)
      {
	  set_error(&error_desc, -ADMIND_ERR_INRECOVERY, NULL);
	  evmgr_command_end(cmd->uid, &error_desc);
	  return;
      }

      /* Being starting here with goal stopping means that we already performed
       * the stop on this node, thus it is in state 'STOPPING'
       * FIXME As you can see the stopping state does not really exist,
       * would be really easy to have one... */
      if (adm_get_state() == ADMIND_STARTING
	  && adm_cluster.goal == ADM_CLUSTER_GOAL_STOPPED)
      {
	  set_error(&error_desc, -EXA_ERR_ADMIND_STOPPING, NULL);
	  evmgr_command_end(cmd->uid, &error_desc);
	  return;
      }

      /* CAREFUL, no break is on purpose: if the stop is accepted we send it
       * to the worker thread. */

    default:
      error_val = make_worker_thread_exec_command(evmgr_mh, cmd->uid,
				  cmd->code, cmd->data, cmd->data_size);
      if (error_val)
      {
	set_error(&error_desc, error_val, NULL);
	evmgr_command_end(cmd->uid, &error_desc);
	return;
      }
      /* When the command is successfully scheduled, the evmgr_command_end
       * is performed when the EXAMSG_ADM_CLUSTER_CMD_END message is
       * received */
      break;
  }
}

/**
 * Handle the init event and start all daemons by calling the recovery init.
 * The only messages possible comming from the cli is EXAMSG_ADM_CLINIT or
 * EXAMSG_ADM_CLSHUTDOWN if the selector thread does its job correctly.
 *
 * \param     msg   Event message
 * \param[in] from  Event source
 */
static void
evmgr_node_stopped_analyze_event(const Examsg *msg, const ExamsgMID *from)
{
  exa_nodeset_t dest_node;
  int error_val;

  exa_nodeset_single(&dest_node, from->netid.node);

  switch (msg->any.type)
  {
    case EXAMSG_ADM_CLUSTER_CMD:
        evmgr_handle_cluster_cmd((const command_t *)msg->payload);
        break;

    case EXAMSG_ADM_CLUSTER_CMD_END:
      {
	command_end_t *command_end = (command_end_t *)msg->payload;

	if (pending_cli_cmd_uid == command_end->cuid)
	  pending_cli_cmd_uid = CMD_UID_INVALID;

	evmgr_command_end(command_end->cuid, &command_end->err_desc);
      }
      break;

    case EXAMSG_ADM_CLSHUTDOWN:	/* Sent by CLI exa_clshutdown WTx */
      if (adm_get_state() == ADMIND_STOPPED)
        {
	  evmgr_ack_cli(-EXA_ERR_ADMIND_STOPPED, msg);
	  break;
	}

      error_val = evmgr_process_shutdown();
      evmgr_ack_cli(error_val, msg);
      break;

    case EXAMSG_ADM_CLDELETE:
      adm_set_state(ADMIND_NOCONFIG);
      conf_delete();
      evmgr_ack_cli(EXA_SUCCESS, msg);
      break;

    /* we can receive those messages when stopping because a race exists
     * between the time we perform the stop and the moment when the message
     * was sent */
    case EXAMSG_SUP_MSHIP_CHANGE:
    case EXAMSG_EVMGR_INST_EVENT:
    case EXAMSG_EVMGR_MSHIP_READY:
    case EXAMSG_EVMGR_MSHIP_YES:
    case EXAMSG_EVMGR_MSHIP_PREPARE:
    case EXAMSG_EVMGR_MSHIP_ACK:
    case EXAMSG_EVMGR_MSHIP_COMMIT:
    case EXAMSG_EVMGR_MSHIP_ABORT:
    case EXAMSG_EVMGR_RECOVERY_REQUEST:
    case EXAMSG_EVMGR_RECOVERY_END:
      exalog_debug("Ignoring message of type %s (%d) which is out of context",
		   examsgTypeName(msg->any.type), msg->any.type);
      break;

    default:
      evmgr_ack_cli(adm_get_state() == ADMIND_STOPPED ? -EXA_ERR_ADMIND_STOPPED :
	            -EXA_ERR_ADMIND_STOPPING, msg);
      exalog_error("Cannot handle this message of type %s (%d) when stopped",
		   examsgTypeName(msg->any.type), msg->any.type);
      break;
  }
}

/**
 * Analyse an event in preparation of the future processing
 * done in the event loop.
 *
 * \param     msg   Event message
 * \param[in] from  Event source
 */
static void
evmgr_analyze_event(const Examsg *msg, const ExamsgMID *from)
{

  switch (msg->any.type)
    {
    case EXAMSG_ADM_CLUSTER_CMD:
      evmgr_handle_cluster_cmd((const command_t *)msg->payload);
      break;

    case EXAMSG_ADM_CLUSTER_CMD_END:
      {
	command_end_t *command_end = (command_end_t *)msg->payload;

	evmgr_command_end(command_end->cuid, &command_end->err_desc);
      }
      break;

    case EXAMSG_EVMGR_INST_EVENT:
      {
	const instance_event_t *inst_event = &((instance_event_msg_t *)msg)->event;

	exalog_trace("Receive EXAMSG_EVMGR_INST_EVENT for %s with %d",
	    examsgIdToName(inst_event->id), inst_event->state);

	/* Ignore check from nodes that are seen down. See bug #3157
	 * FIXME: this is needed as long as the vrt/nbd needs to use EXAMSG_ALLHOST */
	if (exa_nodeset_contains(evmgr_mship(), from->netid.node))
	  handle_instance_event(inst_event);
	else
	  exalog_debug("Ignoring instance event from %s on %d",
	               examsgIdToName(inst_event->id),
		       from->netid.node);
      }
      break;

    case EXAMSG_SUP_MSHIP_CHANGE:
      evmgr_mship_received_local(evmgr_mh, (SupEventMshipChange *)msg);
      break;

    case EXAMSG_EVMGR_MSHIP_READY:
      evmgr_mship_received_ready(evmgr_mh, (evmgr_ready_msg_t *)msg,
				 from->netid.node);
      break;

    case EXAMSG_EVMGR_MSHIP_YES:
      evmgr_mship_received_yes(evmgr_mh, (evmgr_yes_msg_t *)msg,
			       from->netid.node);
      break;

    case EXAMSG_EVMGR_MSHIP_PREPARE:
      evmgr_mship_received_prepare(evmgr_mh, (evmgr_prepare_msg_t *)msg,
				   from->netid.node);
      break;

    case EXAMSG_EVMGR_MSHIP_ACK:
      evmgr_mship_received_ack(evmgr_mh, (evmgr_ack_msg_t *)msg,
			       from->netid.node);
      break;

    case EXAMSG_EVMGR_MSHIP_COMMIT:
      if (evmgr_mship_received_commit(evmgr_mh, (evmgr_commit_msg_t *)msg,
				      from->netid.node))
	if (evmgr_has_quorum())
	  {
	    in_recovery = true;
	    cli_end_timeout = -1;
	  }

      /* There was a commit, this means that the membership is changing.
       * If the current node is waiting for a start, it can wait some more
       * because it may be the next node to be integrated. */
      if (cli_end_timeout > 0)
	cli_end_timeout = TIMEOUT_CLWAITSTART;
      break;

    case EXAMSG_EVMGR_MSHIP_ABORT:
      evmgr_mship_received_abort(evmgr_mh, (evmgr_abort_msg_t *)msg,
				 from->netid.node);
      break;

    case EXAMSG_EVMGR_RECOVERY_END:
	{
	  cl_error_desc_t *err_desc =
	      &((recovery_end_t *)msg->payload)->error_desc;

	  /* There is a race when a node reboots, if the recovery down it
	   * triggered took really a long time and the node completely
	   * rebooted before the down was finished, the leader may send
	   * a end message to the node which is out of context. Here we
	   * ignore the message when it is obviously out of context (if
	   * the in_recovery flag is not set, the node is not performing
	   * a recovery and thus can't possibly finish one). If the message
           * is not ignored here, the node may mark itself as STARTED when
           * it is actually not even in any committed mship, which is BAD! */
	  if (!in_recovery)
	      break;

	  adm_set_state(ADMIND_STARTED);
	  in_recovery = false;

	  evmgr_command_end(pending_cli_cmd_uid, err_desc);
	  pending_cli_cmd_uid = CMD_UID_INVALID;
	}
      break;

    case EXAMSG_EVMGR_RECOVERY_REQUEST:
      /* FIXME why the quorated condition not checked ? */
      if (adm_leader_id == adm_my_id && adm_leader_set)
      {
	need_recovery_set(true);
        in_recovery = true;
      }
      break;

    default:
      evmgr_ack_cli(adm_get_state() == ADMIND_STARTED ? -EXA_ERR_ADMIND_STARTED :
	            -EXA_ERR_ADMIND_STARTING, msg);
      exalog_error("In event loop `evmgr_analyze_event':"
	                 "message malformed or not supported yet: %s (%d)",
			 examsgTypeName(msg->any.type), msg->any.type);
      break;
    }

  /* Admind can only be started if it has the quorum and thus if it doesn't
   * have the quorum at this point, it means the quorum has been lost and
   * the node must be rebooted.
   * In case the loss of quorum happened during a recovery (admind was starting
   * for example bug #3007) the node has to crash in the same way.
   * In any other state this is useless because we do not need the quorum
   */
  if (!evmgr_has_quorum() && (adm_get_state() == ADMIND_STARTED || in_recovery))
    crash_need_reboot("Lost the quorum.");
}

void
evmgr_process_init(cmd_uid_t cuid, cl_error_desc_t *err_desc)
{
  EXA_ASSERT(err_desc);

  set_error(err_desc, evmgr_mship_init(), NULL);
  if (get_error_type(-err_desc->code) == ERR_TYPE_ERROR)
      return;

  set_error(err_desc, examsg_init(), NULL);
  if (err_desc->code != EXA_SUCCESS)
    return;

  /* Here the loop passed is evmgr_node_stopped_analyze_event because csupd is
   * not yet started, so we do not care about clusterized messages */
  adm_hierarchy_call_wt(EXA_ADM_CLINIT, cuid, NULL, 0,
                        evmgr_node_stopped_analyze_event, err_desc);
  if (err_desc->code != EXA_SUCCESS)
  {
    exalog_debug("hierarchy init failed: %s (%d)", err_desc->msg, err_desc->code);
    examsg_shutdown(evmgr_mh);
    return;
  }

  adm_set_state(ADMIND_STARTING);
  cli_end_timeout = TIMEOUT_CLWAITSTART;

  /* Once csupd started, the cluster is 'live', this means that some
   * membership events will come from csupd and trigger the recovery up
   * by setting the 'need_recovery' flag */
  set_error(err_desc, csupd_init(), NULL);
}

static int
evmgr_process_shutdown(void)
{
  cl_error_desc_t err_desc;

  /* XXX Here or at the end of the shutdown? (symmetry) */
  evmgr_mship_shutdown();

  /* Once csupd is stopped, the node is not anymore in clusterized context */
  csupd_shutdown();

  /* Here the loop passed is evmgr_node_stopped_analyze_event because csupd is
   * dead, so we do not care about clusterized messages */
  adm_hierarchy_call_wt(EXA_ADM_RUN_SHUTDOWN, CMD_UID_INVALID, NULL, 0,
			evmgr_node_stopped_analyze_event, &err_desc);
  if (err_desc.code != EXA_SUCCESS)
    exalog_error("hierarchy init returned %d", err_desc.code);
  /* do not return on error as there is nothing to do as trying to stop... */

  /* Set all instances to down */
  /* Careful: there is a disymetry between nodes that are actually stopping
   * and those that are being kept up; the one that are being stopped have to
   * reset all instances status (because from there point of view, all nodes
   * are down) whereas nodes that are still up only see some nodes down.
   * So reset is done here in any case because if a node executes this part of
   * the code, this means that it is actually stopping. */
  inst_set_all_instances_down();

  examsg_shutdown(evmgr_mh);

  if (err_desc.code == EXA_SUCCESS)
    adm_set_state(ADMIND_STOPPED);

  return err_desc.code;
}

void
evmgr_process_events(void)
{
  if (need_recovery_get() && adm_cluster.goal == ADM_CLUSTER_GOAL_STARTED)
   {
      inst_op_t op;
      cl_error_desc_t err_desc;

      EXA_ASSERT_VERBOSE(adm_is_leader() && evmgr_has_quorum(),
	                 "Something went wrong: I am %sthe leader and"
	                 " I am %squorated", adm_is_leader() ? "" : "not ",
			 evmgr_has_quorum() ? "" : "not ");

      cli_end_timeout = -1;

      exalog_trace("adm_hierarchy_run_recovery START");

      while ((op = inst_compute_recovery()) != INST_OP_NOTHING)
      {
        /* There MUST be a recovery expected */
        EXA_ASSERT(need_recovery_get());

	/* FIXME when automatically triggered recovery, the pending_cli_cmd_uid
	 * remains INVALID. It would probably be a good idea to have a special
	 * valid id to be passed here in order to be able to tell real invalid
	 * data, from not triggered from cli. */
        adm_hierarchy_call_wt(EXA_ADM_RUN_RECOVERY, pending_cli_cmd_uid,
			      &op, sizeof(op),
			      evmgr_analyze_event, &err_desc);
      }

      /* The need recovery flag is systematically reset because if the recovery
       * thread says that there is nothing to do, there is actually nothing to
       * do. As a matter of fact, the need_recovery must be set to be able to
       * perform a recovery, but having the need_recovery set does not mean
       * that a recovery is needed. For example if csupd commits twice the same
       * mship, the flag is set, when there is actually nothing to do. */
      need_recovery_set(false);

      in_recovery = false;

      adm_set_state(ADMIND_STARTED);

      evmgr_recovery_end(&err_desc);

      evmgr_command_end(pending_cli_cmd_uid, &err_desc);
      pending_cli_cmd_uid = CMD_UID_INVALID;
    }
}

/**
 * Let all nodes know that the recovery is over.
 * Local nodes can then reset their in_recovery flag and ack the CLI (if
 * needed).
 *
 * \param[in] error_desc  Error descriptor (result of recovery)
 */
static void
evmgr_recovery_end(const cl_error_desc_t *error_desc)
{
  ExamsgAny header;
  recovery_end_t msg;
  int s;
  exa_nodeset_t recovery_mship;

  /* Signal the end of the recovery to all nodes that participated;
   * no need to send a message locally since we know the recovery
   * is over. */
  exa_nodeset_copy(&recovery_mship, evmgr_mship());
  exa_nodeset_del(&recovery_mship, adm_my_id);

  exalog_trace("cli_end: '%s' (%d), mship=" EXA_NODESET_FMT,
               error_desc->msg, error_desc->code,
	       EXA_NODESET_VAL(&recovery_mship));

  header.type = EXAMSG_EVMGR_RECOVERY_END;
  msg.error_desc = *error_desc;

  s = examsgSendWithHeader(evmgr_mh, EXAMSG_ADMIND_EVMGR_ID, &recovery_mship,
                           &header, &msg, sizeof(msg));
  if (s != sizeof(msg))
    exalog_error("failed to inform " EXA_NODESET_FMT
                 " that the recovery is finished (s=%d '%s')",
                 EXA_NODESET_VAL(&recovery_mship), s, exa_error_msg(s));
}

/**
 * Tells whether a recovery is in progress or whether a recovery will
 * be started soon.
 */
int
evmgr_is_recovery_in_progress(void)
{
  return in_recovery;
}

void
evmgr_request_recovery(ExamsgHandle mh)
{
  ExamsgAny msg;
  int rv;

  msg.type = EXAMSG_EVMGR_RECOVERY_REQUEST;

  rv = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, EXAMSG_LOCALHOST,
		  &msg, sizeof(msg));

  /* FIXME: Proper error-handling would be much better. */
  EXA_ASSERT(rv == sizeof(msg));
}
