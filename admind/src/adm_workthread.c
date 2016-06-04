/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_error.h"
#include "os/include/os_semaphore.h"
#include "os/include/os_time.h"
#include "os/include/os_atomic.h"

#include "common/include/exa_constants.h"
#include "common/include/threadonize.h"
#include "os/include/os_mem.h"

#include "admind/src/adm_workthread.h"
#include "admind/src/cli_server.h"
#include "admind/src/evmgr/evmgr.h"

#include "log/include/log.h"

struct t_work
{
  /** The pthread structure initialized at the thread creation */
  os_thread_t thread;

  worker_thread_id_t thr_nb; /**< The id of the thread that is authorized to access this */

  /** The ExamsgHandle used by the thread to send and receive
      messages */
  ExamsgHandle mb_inbox;

  /** The ExamsgHandle used by local commands to do their work. */
  ExamsgHandle mb_local;

  /** The ExamsgHandle used for barrier. Two are needed */
  ExamsgHandle mb_barrier[2];

  /** The uid of the command that was received. When work is finished, this
   * id is sent back to the caller with the result; the caller can then match
   * this uid with the request */
  cmd_uid_t cmd_uid;

  admwrk_ctx_t *admwrk;

  ExamsgID id_mine;
  bool stop;
};

/**
 * The state of a working thread.
 * This is TLS __AND__ STATIC and __MUST__ remain so.
 */
static __thread t_work *thr = NULL;

static const char *work_thread_names[] = {
  [CLINFO_THR_ID]     = "Info",
  [CLICOMMAND_THR_ID] = "Command",
  [RECOVERY_THR_ID]   = "Recovery"
};

static void work_thread(/* thr is a (t_work *) */ void *thr);

ExamsgHandle
adm_wt_get_inboxmb(void)
{
  return thr->mb_inbox;
}

ExamsgHandle
adm_wt_get_localmb(void)
{
  return thr->mb_local;
}

ExamsgHandle adm_wt_get_barmb(bool even)
{
  return thr->mb_barrier[even ? 1: 0];
}

admwrk_ctx_t *adm_wt_get_admwrk_ctx(void)
{
  return thr->admwrk;
}

const char *
adm_wt_get_name(void)
{
  return work_thread_names[thr->thr_nb];
}

int
launch_worker_thread(t_work **thr_ptr, worker_thread_id_t thr_id,
                     ExamsgID id_mine, ExamsgID id_local,
                     ExamsgID id_barrier_odd, ExamsgID id_barrier_even)
{
  t_work *thr = os_malloc(sizeof(t_work));
  int retval;

  EXA_ASSERT(THREAD_ID_IS_VALID(thr_id));

  exalog_debug("Create work_thread %d", thr_id);

  thr->stop = false;
  thr->thr_nb  = thr_id;
  thr->cmd_uid = CMD_UID_INVALID;
  thr->id_mine = id_mine;

  thr->admwrk = admwrk_ctx_alloc();
  if (thr->admwrk == NULL)
    return -ENOMEM;

  /* initialize examsg framework */
  thr->mb_inbox = examsgInit(id_mine);
  if (!thr->mb_inbox)
    return -EINVAL;

  thr->mb_local = examsgInit(id_local);
  if (!thr->mb_local)
    return -EINVAL;

  /* create local mailboxes */
  retval = examsgAddMbox(thr->mb_inbox, examsgOwner(thr->mb_inbox),
	                 EXA_MAX_NODES_NUMBER, ADM_MAILBOX_PAYLOAD_PER_NODE);
  if (retval)
    return retval;

  retval = examsgAddMbox(thr->mb_local, examsgOwner(thr->mb_local),
	                 2, EXAMSG_MSG_MAX);
  if (retval)
    return retval;

  /* This is the ExamsgID that the thread is waiting on. */

  thr->mb_barrier[0] = examsgInit(id_barrier_even);
  if (!thr->mb_barrier[0])
    return -EINVAL;

  thr->mb_barrier[1] = examsgInit(id_barrier_odd);
  if (!thr->mb_barrier[1])
    return -EINVAL;

  retval = examsgAddMbox(thr->mb_barrier[0], examsgOwner(thr->mb_barrier[0]),
	                 EXA_MAX_NODES_NUMBER, ADM_MAILBOX_PAYLOAD_PER_NODE);
  if (retval)
    return retval;

  retval = examsgAddMbox(thr->mb_barrier[1], examsgOwner(thr->mb_barrier[1]),
	                 EXA_MAX_NODES_NUMBER, ADM_MAILBOX_PAYLOAD_PER_NODE);
  if (retval)
    return retval;

  /* The mutex is held because thr->thread is accessed */
  if (!exathread_create_named(&thr->thread,
                              ADMIND_THREAD_STACK_SIZE+MIN_THREAD_STACK_SIZE,
                              &work_thread, thr, work_thread_names[thr->thr_nb]))
      return -EXA_ERR_DEFAULT;

  *thr_ptr = thr;

  return EXA_SUCCESS;
}

void stop_worker_thread(t_work *thr)
{
    ExamsgAny msg;

    EXA_ASSERT(thr != NULL);
    thr->stop = true;

    /* Send an empty message to unblock examsgWait() */
    msg.type = EXAMSG_ADM_CLUSTER_CMD;
    examsgSend(thr->mb_inbox, thr->id_mine, EXAMSG_LOCALHOST, &msg, sizeof(msg));

    os_thread_join(thr->thread);

    examsgDelMbox(thr->mb_inbox, examsgOwner(thr->mb_inbox));
    examsgDelMbox(thr->mb_local, examsgOwner(thr->mb_local));
    examsgDelMbox(thr->mb_barrier[0], examsgOwner(thr->mb_barrier[0]));
    examsgDelMbox(thr->mb_barrier[1], examsgOwner(thr->mb_barrier[1]));
    examsgExit(thr->mb_inbox);
    examsgExit(thr->mb_local);
    examsgExit(thr->mb_barrier[0]);
    examsgExit(thr->mb_barrier[1]);

    admwrk_ctx_free(thr->admwrk);

    os_free(thr);
}

/**
 * \brief return a command id
 * return a command id != CMD_UID_INVALID
 * This function NEVER returns CMD_UID_INVALID
 * This function is thread safe and never blocks.
 */
cmd_uid_t
get_new_cmd_uid(void)
{
  static os_atomic_t count = { .val = 0 };
  int value, new_value;
  do {
      value = os_atomic_read(&count);

      new_value = value + 1;
      if (new_value == (int)CMD_UID_INVALID)
         new_value++;

  } while(os_atomic_cmpxchg(&count, value, new_value) != value);

  return (cmd_uid_t)new_value;
}

/**
 * Make a working thread execute an XML command.
 *
 * \param[in] mh	Examsg handle of the caller.
 * \param[in] uid       Command id given by the caller to be able to identify
 *                      the request.
 * \param[in] cmd_code  Code of the ADM command
 * \param[in] data      Data for the command
 *
 * \return 0 if successfull, a negative number otherwise
 *           -EXA_ERR_ADM_BUSY means that a command is already running
 *           If this function returns an error the request was NOT scheduled.
 */
int
make_worker_thread_exec_command(ExamsgHandle mh, cmd_uid_t uid,
                                adm_command_code_t cmd_code,
				const void *data, size_t data_size)
{
  Examsg msg;
  ExamsgID to;
  command_t *cmd = (command_t *)msg.payload;
  size_t payload_size;
  int s;

  /* CLINFO/GETCONFIG has it's very own thread */
  if (cmd_code == EXA_ADM_CLINFO || cmd_code == EXA_ADM_CLSTATS)
    to = EXAMSG_ADMIND_INFO_ID;
  else if (cmd_code == EXA_ADM_CLINIT
	   || cmd_code == EXA_ADM_RUN_RECOVERY
	   || cmd_code == EXA_ADM_RUN_SHUTDOWN
	   || cmd_code == EXA_ADM_CLNODESTOP)
    to = EXAMSG_ADMIND_RECOVERY_ID;
  else
    to = EXAMSG_ADMIND_CMD_ID;

  /* Message to trigger the command execution on thread thr_nb */
  msg.any.type = EXAMSG_ADM_CLUSTER_CMD;

  payload_size = data_size + sizeof(command_t);

  if (payload_size >= EXAMSG_PAYLOAD_MAX)
    {
      exalog_error("Command %s parameter structure is too big to fit in "
	  "a message. %" PRIzu " > %" PRIzu , adm_command_name(cmd_code),
	  payload_size, EXAMSG_PAYLOAD_MAX);
      return -E2BIG;
    }

  cmd->code = cmd_code;
  cmd->uid  = uid;
  memcpy(cmd->data, data, data_size);
  cmd->data_size = data_size;

  exalog_debug("Scheduling command '%s', uid=%d",
               adm_command_name(cmd_code), cmd->uid);

  s = examsgSend(mh, to, EXAMSG_LOCALHOST,
	         &msg, sizeof(ExamsgAny) + payload_size);
  if (s != sizeof(ExamsgAny) + payload_size)
    return s;

  return 0;
}

/** \brief Special local command that executes a cluster command.
 *
 * \param[in] msg: msg received
 * \return EXA_SUCCESS.
 *
 */
static void exec_cluster_cmd(command_t *msg, cl_error_desc_t *err_desc)
{
  const AdmCommand *cmd;

  cmd = adm_command_find(msg->code);
  if (!cmd)
    {
      exalog_error("Command code '%d' is out of bounds (thread=%d)",
		  msg->code, thr->thr_nb);
      return;
    }

  exalog_debug("Thread %s: received cluster command '%s'",
               adm_wt_get_name(), cmd->msg);

  /* Delay the command if there is a recovery in progress */
  while (evmgr_is_recovery_in_progress() && !cmd->allowed_in_recovery)
  {
    exalog_debug("recovery in progress: delay the command");
    os_sleep(1);
  }

  /* Call the cluster command */
  cmd->cluster_command(thr->admwrk, msg->data, err_desc);

  exalog_debug("%s: %s (%d)", cmd->msg,
    err_desc->msg[0] != '\0' ? err_desc->msg : exa_error_msg(err_desc->code),
    err_desc->code);
}

static void
command_complete(ExamsgMID *from, const command_t *cmd,
                 cl_error_desc_t *err_desc)
{
  int ret;
  ExamsgAny header;
  command_end_t command_end;

  header.type = EXAMSG_ADM_CLUSTER_CMD_END;

  command_end.err_desc = *err_desc;
  command_end.cuid      = cmd->uid;

  ret = examsgSendWithHeader(thr->mb_inbox, from->id, EXAMSG_LOCALHOST,
                             &header, &command_end, sizeof(command_end));

  EXA_ASSERT_VERBOSE(ret == sizeof(command_end), "Command reply failed '%d'", ret);
  /* FIXME check return value */
}

void work_thread_handle_msg(Examsg *msg, ExamsgMID *from)
{
  /* Prevent the thread to handle multiple commands at the same time */
  static __thread bool in_use = false;
  switch (msg->any.type)
    {
      case EXAMSG_ADM_CLUSTER_CMD:
	{
	  cl_error_desc_t err_desc;
	  command_t *command = (command_t *)msg->payload;

	  if (in_use)
	  {
	      set_error(&err_desc, -EXA_ERR_ADM_BUSY, NULL);
	      command_complete(from, command, &err_desc);
	      return;
	  }

	  in_use = true;

	  /* set a default guard message before calling command, in case
	   * err_desc is not set by the command; */
	  set_error(&err_desc, -EXA_ERR_DEFAULT,
		  "Unknown outcome for command '%s'.",
		  adm_command_name(command->code));

	  thr->cmd_uid = command->uid;

	  exec_cluster_cmd(command, &err_desc);

	  in_use = false;
	  thr->cmd_uid = 0;

	  /*
	   * Now that our thread is free to accept a new command, we can tell
	   * the CLI/GUI the response (Doing it earlier creates a race, the
	   * CLI/GUI may send us another request while we are not completly
	   * ready).
	   */
	  command_complete(from, command, &err_desc);
	}
	break;

      case EXAMSG_SERVICE_LOCALCMD:
	/* FIXME the in use flag should probably be set here too */
	admwrk_handle_localcmd_msg(thr->admwrk, msg, from);
	break;

      default:
	exalog_error("Worker thread cannot handle message of type '%d'",
	             msg->any.type);
	break;
    }
}


static int
work_thread_receive(t_work *thr, Examsg *msg, ExamsgMID *from)
{
  int s;

  do {
      s = examsgWait(thr->mb_inbox);
      if (s != 0)
	  return s;
  } while ((s = examsgRecv(thr->mb_inbox, from, msg, sizeof(*msg))) == 0);

  if (s < 0)
      return s;

  return EXA_SUCCESS;
}

static void
work_thread(void *data)
{
  /* thr is in TLS */
  thr = data;

  EXA_ASSERT_VERBOSE(THREAD_ID_IS_VALID(thr->thr_nb),
                     "invalid thread id '%d'", thr->thr_nb);

  /* Initialize exalog */
  exalog_as(examsgOwner(thr->mb_inbox));

  while(!thr->stop)
  {
      Examsg msg;
      ExamsgMID from;

      int ret = work_thread_receive(thr, &msg, &from);
      if (thr->stop)
          break;

      if (ret != 0)
	  exalog_error("Thread %s failed to receive a message: %s (%d).",
		       adm_wt_get_name(), exa_error_msg(ret), ret);

      work_thread_handle_msg(&msg, &from);
  }

  thr = NULL;
}

/* FIXME In the stuff coming next there is a direct answer from the WT to the
 * CLI ; in a perfect world, the WT would send a examsg to the CLI thread that
 * would pipe it in the cli socket. */
char *adm_cli_ip(void)
{
    return cli_get_peername(thr->cmd_uid);
}

void adm_write_inprogress(const char *src_node, const char *desc,
	                  int err, const char *str)
{
    cli_write_inprogress(thr->cmd_uid, src_node, desc, err, str);
}

void send_payload_str(const char *str)
{
    cli_payload_str(thr->cmd_uid, str);
}

