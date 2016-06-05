/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/rpc.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"

#include <string.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/instance.h"
#include "admind/src/adm_command.h"
#include "admind/src/rpc_command.h"
#include "admind/src/adm_workthread.h"
#include "log/include/log.h"

/** Structure to handle RPC replies */
typedef struct admwrk_request_t
{
  exa_nodeset_t waiting_for;  /**< bitmap of the nodes we are waiting for */
  ExamsgHandle mh;            /**< Examsg handle to use */
} admwrk_request_t;

typedef struct rpc_cmd {
  exa_nodeset_t mship;
  rpc_command_t command; /**< rpc command id */
  uint32_t      pad;
  char          data[];  /**< payload given by caller */
} rpc_cmd_t;

struct rpc_barrier_data {
  int  err;
  int  pad;
  char error_msg[EXA_MAXSIZE_ERR_MESSAGE + 1];
};

typedef struct barrier {
  int           rank;
  exa_nodeset_t nodes;
} barrier_t;

typedef struct admwrk_ctx_t
{
  barrier_t bar; /**< barrier private stuff */
  admwrk_request_t rpc;

  char reply[EXAMSG_PAYLOAD_MAX]; /**< Reply data of the local command */
  size_t reply_size;              /**< Size of the reply */
  bool (*inst_is_node_down)(exa_nodeid_t nid);
  void (*inst_get_current_membership)(const struct adm_service *service,
                                 exa_nodeset_t *membership);
} admwrk_ctx_t;

admwrk_ctx_t *admwrk_ctx_alloc()
{
    return os_malloc(sizeof(admwrk_ctx_t));
}

void admwrk_ctx_free(admwrk_ctx_t *ctx)
{
   os_free(ctx);
}

/**
 * Send an examsg. Same as examsgSend(), but that always use examsg loopback
 * and never use the network when the membership contains only the local node.
 * The goal is to use admwrk_*() functions when the network is down.
 *
 * Same prototype as examsgSend().
 */
static int
admwrk_send(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
            ExamsgType type, const void *buffer, size_t nbytes)
{
  ExamsgAny header;
  int count;

  count = exa_nodeset_count(dest_nodes);
  if (count == 0)
    {
      exalog_trace("drop");
      return nbytes;
    }

  header.type = type;

  return examsgSendWithHeader(mh, to, dest_nodes, &header, buffer, nbytes);
}


/**
 * Local execution of a RPC command.
 *
 * Execute a local command with information described by the rpc_cmd_t param.
 *
 * @param[in]  ctx      current thread number
 * @param[in]  admwrk_cmd  data/metadata of the rpc command.
 *
 * Upon returning, the reply is available in ctx->reply.
 * FIXME this is really ugly. A output buffer should probably be provided
 * to the local command.
 */
static void admwrk_exec_local_cmd(admwrk_ctx_t *ctx, const rpc_cmd_t *admwrk_cmd)
{
  LocalCommand localCommand;

  EXA_ASSERT(exa_nodeset_contains(&admwrk_cmd->mship, adm_my_id));

  /* Init barrier private stuff */
  ctx->bar.rank  = 0;
  ctx->bar.nodes = admwrk_cmd->mship;

// FIXME  if (ctx == RECOVERY_THR_ID)
//      ctx->inst_is_node_down = inst_is_node_down_rec;
//  else
//      ctx->inst_is_node_down = inst_is_node_down_cmd;

  /* find the local command */
  localCommand = rpc_command_get(admwrk_cmd->command);

  /* Execute the command */
  localCommand(ctx, (void *)admwrk_cmd->data);
}

/**
 * Handle and unpack a EXAMSG_SERVICE_LOCALCMD message.
 *
 * A node called admwrk_run_command which sent a message to the nodes that
 * are supposed to perform the local command. When a node receives the
 * EXAMSG_SERVICE_LOCALCMD message, it just has to call this function in order
 * to execute the local command
 *
 * @param[in]  ctx  current thread number
 * @param[in]  msg     EXAMSG_SERVICE_LOCALCMD message containing the rpc_cmd_t
 *                     command and data.
 * @param[in]  from    ExamsgMID of the requester
 */
void admwrk_handle_localcmd_msg(admwrk_ctx_t *ctx, const Examsg *msg, ExamsgMID *from)
{
  exa_nodeset_t dest_nodes;
  int ret;

  EXA_ASSERT(msg->any.type == EXAMSG_SERVICE_LOCALCMD);

  /* The payload of the examsg is a rpc_cmd_t */
  admwrk_exec_local_cmd(ctx, (rpc_cmd_t *)msg->payload);

  exalog_debug("<<<%s>>> send ack to request", adm_wt_get_name());
  EXA_ASSERT(strcmp(from->host, ""));
  EXA_ASSERT(from->netid.node != EXA_NODEID_LOCALHOST
	     && from->netid.node != adm_my_id);

  /* admwrk_exec_local_cmd already put the reply in the ctx->reply buffer */
  exa_nodeset_single(&dest_nodes, from->netid.node);
  ret = admwrk_send(adm_wt_get_inboxmb(), from->id, &dest_nodes,
	  EXAMSG_SERVICE_REPLY, ctx->reply, ctx->reply_size);
  EXA_ASSERT_VERBOSE(ret == ctx->reply_size, "admwrk_send() returned %d", ret);
}

/* --- admwrk_run_command ----------------------------------------- */

/**
 * Run a local command on all nodes. Do not block. The caller should
 * get individual replies with admwrk_get_ack().
 *
 * @param[in]  ctx     current thread number
 * @param[in]  service    the service that run this command
 * @param[out] handle     handle for this RPC (it should be allocated on the
 *                        caller stack)
 * @param[in]  command    the id of the command to run
 * @param[in]  __request  arbitrary structure containing the request parameters
 * @param[in]  size       size of request
 */
void
admwrk_run_command(admwrk_ctx_t *ctx,
		   exa_nodeset_t *nodes, int command,
		   const void *__request, size_t size)
{
  char buffer[sizeof(rpc_cmd_t) + size];
  rpc_cmd_t *request = (rpc_cmd_t *)buffer;
  admwrk_request_t *handle = &ctx->rpc;
  int ret;

  memcpy(request->data, __request, size);

  /* Initialize the handle */

  /* set the failure detector. It is specific for each thread as the
   * recovery thread just needs to be informed about down events and
   * the other threads need to know about instances that are actually down */
// FIXME  if (ctx == RECOVERY_THR_ID)
//      handle->is_node_down = inst_is_node_down_rec;
//  else
//      handle->is_node_down = inst_is_node_down_cmd;

  handle->mh   = adm_wt_get_inboxmb();
  handle->waiting_for = *nodes;

  request->mship   = *nodes;
  request->command = command;

  /* Send the request */

  exalog_debug("%s send command %d with membership " EXA_NODESET_FMT,
	       adm_wt_get_name(), command, EXA_NODESET_VAL(nodes));

  exa_nodeset_del(nodes, adm_my_id);

  ret = admwrk_send(adm_wt_get_inboxmb(),
		    examsgOwner(adm_wt_get_inboxmb()), nodes,
		    EXAMSG_SERVICE_LOCALCMD, request, sizeof(*request) + size);
  EXA_ASSERT_VERBOSE(ret == sizeof(*request) + size,
		     "admwrk_send() returned %d", ret);

  /* Execute myself the local command */
  admwrk_exec_local_cmd(ctx, request);
}


/* --- admwrk_exec_command ---------------------------------------- */

/**
 * Run a local command on all nodes and returns its global status.
 *
 * @param[in]  ctx     current thread number
 * @param[in]  service    the service that run this command
 * @param[in]  command    the id of the command to execute
 * @param[in]  request    arbitrary structure containing the request parameters
 * @param[in]  size       size of request parameters
 *
 * @return     -ADMIND_ERR_NODE_DOWN if one of the node in the
 *             membership is DOWN, EXA_SUCCESS if all the nodes return
 *             EXA_SUCCESS, one of the error codes returned by the
 *             faulting nodes else.
 */
int
admwrk_exec_command(admwrk_ctx_t *ctx, const struct adm_service *service,
	            int command, const void *request, size_t size)
{
  exa_nodeid_t nodeid;
  int ret = EXA_SUCCESS;
  int err;

  exa_nodeset_t nodes;
  ctx->inst_get_current_membership(service, &nodes);

  admwrk_run_command(ctx, &nodes, command, request, size);

  while (!exa_nodeset_is_empty(&nodes))
  {
    admwrk_get_ack(ctx, &nodes, &nodeid, &err);
    exalog_debug("%s: '%s' acked with %s (%d) to command %d",
		 adm_wt_get_name(), adm_cluster_get_node_by_id(nodeid)->name,
		 exa_error_msg(err), err, command);

    if (err == -ADMIND_ERR_NODE_DOWN)
      ret = -ADMIND_ERR_NODE_DOWN;
    if (ret != -ADMIND_ERR_NODE_DOWN && err != EXA_SUCCESS && err != -ADMIND_ERR_NOTHINGTODO)
      ret = err;
  }

  exalog_debug("%s command %d done with result %d",
	       adm_wt_get_name(), command, ret);

  return ret;
}


/* --- admwrk_bcast ----------------------------------------------- */

/**
 * Broadcast a message to all nodes. To be called in a local command.
 * If each node does a broadcast, each node can get the message from
 * each other by calling admwrk_get_bcast().
 *
 * @param[in]  ctx     current thread number
 * @param[out] handle     handle for this RPC (it should be allocated on the
 *                        caller stack)
 * @param[in]  type       the type id of the message to send
 * @param[in]  out        struct containing the data
 * @param[in]  size       size of the data
 */
void
admwrk_bcast(admwrk_ctx_t *ctx, 
	     int type, const void *out, size_t size)
{
  barrier_t *bar = &ctx->bar;
  admwrk_request_t *handle = &ctx->rpc;
  int ret;

  bar->rank++;

  /* Initialize the handle */

  handle->waiting_for = bar->nodes;
  handle->mh          = adm_wt_get_barmb(bar->rank % 2);

  /* Send the request */

  exalog_debug("%s send bcast rank '%d' with membership "
	       EXA_NODESET_FMT, adm_wt_get_name(), bar->rank,
	       EXA_NODESET_VAL(&handle->waiting_for));

  ret = admwrk_send(handle->mh, examsgOwner(handle->mh),
		    &bar->nodes, type, out, size);

  EXA_ASSERT_VERBOSE(ret == size, "admwrk_send() returned %d", ret);
}


static int
admwrk_recv_msg(ExamsgHandle mh, ExamsgType type, struct timeval *timeout,
                exa_nodeid_t *from, void *buf, size_t size)
{
    Examsg my_msg;
    ExamsgMID mid;
    int ret;

    do {

	/* Wait for a new message and receive it */
	do {
	    ret = examsgWaitTimeout(mh, timeout);
	    if (ret != 0)
		return ret;
	}  while ((ret = examsgRecv(mh, &mid, &my_msg, sizeof(my_msg))) == 0);

	if (ret < 0)
	    return ret;

	/* FIXME this test would be mandatory for a rigourous check, but it
	 * does not work when receiver does not know the amount of data it
	 * waits for
	 *
	 * EXA_ASSERT_VERBOSE(size + sizeof(ExamsgAny) != ret,
	 *                    "invalid size %Zu != %d",
	 *                    size + sizeof(ExamsgAny), ret);
	 */

	/* If the message is not one of those we are waiting for here,
	 * we pass it to the worker thread loop */

	if (my_msg.any.type != type)
	    work_thread_handle_msg(&my_msg, &mid);

    } while (my_msg.any.type != type);

    if (from)
	*from = mid.netid.node;

    memcpy(buf, my_msg.payload, size);

    return EXA_SUCCESS;
}

/**
 * Wait for the next message from one of the whole nodes in the
 * current admind business membership. err will be set to
 * -ADMIND_ERR_NODE_DOWN if the node was down before entering the
 * barrier. admwrk_get_msg() returns true until we got all messages
 * and/or interruptions. One *must* call admwrk_get_msg() until it
 * returns false, it is not allowed to break the loop.
 *
 * @param[out]     handle     handle for this RPC
 * @param[out]     _nodeid    nodeid of the node from wich come the event
 *                            may be NULL if caller does not need the
 *                            information
 * @param[in]      type       identifier of the expected reply type
 * @param[in:out]  buf        Buffer where to store the data received
 * @param[in]      size       size of the buffer
 * @param[out]     err        pointer to an int which stores the status
 *
 * @return     false if we got all replies, true otherwise.
 */
static int
admwrk_get_msg(admwrk_ctx_t *ctx, exa_nodeset_t *nodes, exa_nodeid_t *_nodeid,
	       ExamsgType type, void *buf, size_t size, int *err)
{
#define CHECK_DOWN_TIMEOUT (struct timeval){ .tv_sec = 0, .tv_usec = 100000 }
  struct timeval timeout = CHECK_DOWN_TIMEOUT;
  exa_nodeid_t nodeid = EXA_NODEID_NONE;
  admwrk_request_t *handle = &ctx->rpc;
  int ret;

  exalog_trace("awaiting a message for nodes " EXA_NODESET_FMT,
               EXA_NODESET_VAL(nodes));

  /* Handle the replies and periodically check for down events;
   * The priority is given for reading messages, as this function relies on the
   * fact that the node down event will ALWAYS happen after a message of the
   * node detected down.
   * FIXME this is actually a bad assertion as there is no guaranty that examsg
   * will deliver all messages from a node before the node is seen down (this
   * could be a side effect of retransmit and is possible even if unlikely.
   */
  while ((ret = admwrk_recv_msg(handle->mh, type, &timeout, &nodeid, buf, size)) == -ETIME)
  {
    exalog_trace("%s check NODE_DOWN", adm_wt_get_name());

    exa_nodeset_foreach(nodes, nodeid)
    {
	/* FIXME how can myself be down here ? */
	if (ctx->inst_is_node_down(nodeid) && nodeid != adm_myself()->id)
	{
	    if (_nodeid)
		*_nodeid = nodeid;

	    exalog_debug("Interrupted by a node down of %d", nodeid);

	    exa_nodeset_del(nodes, nodeid);
	    memset(buf, 0, size);
	    *err = -ADMIND_ERR_NODE_DOWN;
	    return true;
	}
    }
    /* well, nothing went wrong, so give it another try to get messages */
    timeout = CHECK_DOWN_TIMEOUT;
  }

  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "examsgWait() returned %d", ret);

  exa_nodeset_del(nodes, nodeid);

  if (_nodeid)
      *_nodeid = nodeid;

  *err = EXA_SUCCESS;
  return true;
}

/* --- admwrk_get_reply ------------------------------------------- */

int
admwrk_get_reply(admwrk_ctx_t *ctx, exa_nodeset_t *nodes, exa_nodeid_t *nodeid,
		 void *reply, size_t size, int *err)
{
  int retval;

  /* Special case for local commands executed on the node that issued
     them. */
  if (adm_nodeset_contains_me(nodes))
  {
    memcpy(reply, ctx->reply, size);
    exa_nodeset_del(nodes, adm_my_id);

    if (nodeid)
	*nodeid = adm_my_id;

    *err = EXA_SUCCESS;
    return true;
  }

  retval = admwrk_get_msg(ctx, nodes, nodeid, EXAMSG_SERVICE_REPLY, reply, size, err);

  return retval;
}

/**
 * Wait for the next ack from one of the whole nodes in the current
 * command membership. err will be set to -ADMIND_ERR_NODE_DOWN if the
 * node was down while executing the command. admwrk_get_ack() returns
 * true until we got all messages and/or interruptions. One *must*
 * call admwrk_get_ack() until it returns false, it is not allowed to
 * break the loop.
 *
 * @param[in] ctx Admind thread number
 *
 * @param[in] handle The admwrk_request handle
 *
 * @param[out] err   The ack
 */
int
admwrk_get_ack(admwrk_ctx_t *ctx, exa_nodeset_t *nodes, exa_nodeid_t *nodeid, int *err)
{
  int ret, success;

  ret = admwrk_get_reply(ctx, nodes, nodeid, err, sizeof(*err), &success);

  if(success != EXA_SUCCESS)
    *err = success;

  return ret;
}


/* --- admwrk_get_bcast ------------------------------------------- */

/**
 * Wait for the next answer from one of the whole nodes in the current
 * command membership, following an admwrk_bcast()
 * call. admwrk_get_bcast() returns true until we got all messages
 * and/or interruptions. One *must* call admwrk_get_bcast() until it
 * returns false, it is not allowed to break the loop.
 *
 * @param[in] handle The admwrk_request handle
 *
 * @param[in] reply  Where to put the reply
 *
 * @param[in] size   Size allowed for the reply
 *
 * @param[out] err   The ack
 */
int
admwrk_get_bcast(admwrk_ctx_t *ctx, exa_nodeid_t *nodeid,
	         ExamsgType type, void *reply, size_t size, int *err)
{
  admwrk_request_t *handle = &ctx->rpc;

  /* Finish the loop if there is no more node to wait for. */
  /* TODO next test is dumb, this is caller stuff to check that */
  if (exa_nodeset_is_empty(&handle->waiting_for))
  {
    exalog_trace("no more node to wait for");
    /* The next memset is here to kill the buffer content.
     * This is here to prevent side effect programming: the caller MUST
     * take the data when it is relevant, and not expect it to be eventually
     * relevent... */
    memset(reply, 0xAA, size);
    *err = EXA_SUCCESS;
    return false;
  }

  return admwrk_get_msg(ctx, &handle->waiting_for, nodeid, type, reply, size, err);
}

/* --- admwrk_reply ---------------------------------------- */

/**
 * Reply to a command without an Examsg. To be used when called by
 * admwrk_run_command().
 *
 * @param[in]  ctx   current thread number
 * @param[in]  reply    data
 * @param[in]  size     size of data
 */
void
admwrk_reply(admwrk_ctx_t *ctx, void *__reply, size_t size)
{
  memcpy(ctx->reply, __reply, size);
  ctx->reply_size = size;
}

/* --- admwrk_barrier_msg ----------------------------------------- */

/**
 * Does a barrier in a local command.
 *
 * @param[in]  ctx      current thread number
 * @param[in]  request     Examsg containing the request parameters
 * @param[in]  from        ExamsgMID of the resquest
 * @param[in]  err         a status that will be sent to all nodes
 * @param[in]  step        a string describing the current step
 * @param[in]  fmt         a printf-like format for the error message
 * @param[in]  ...         printf-like parameters for the error message
 *
 * @return     -ADMIND_ERR_NODE_DOWN if one of the node in the
 *             membership is DOWN, EXA_SUCCESS if all the nodes returns
 *             EXA_SUCCESS, one of the error codes returned by the
 *             faulting nodes else.
 */
int
admwrk_barrier_msg(admwrk_ctx_t *ctx, int err, const char *step, const char *fmt, ...)
{
  exa_nodeid_t nodeid;
  struct rpc_barrier_data msg;
  struct rpc_barrier_data rcv;
  int ret = EXA_SUCCESS;
  va_list ap;
  bool sent_inprogress = false;

  EXA_ASSERT(fmt != NULL);

  /* Build message */
  msg.err = err;

  va_start(ap, fmt);
  os_vsnprintf(msg.error_msg, sizeof(msg.error_msg), fmt, ap);
  va_end(ap);

  if (msg.error_msg[0] == '\0')
    strlcpy(msg.error_msg, exa_error_msg(err), sizeof(msg.error_msg));


  admwrk_bcast(ctx, EXAMSG_SERVICE_BARRIER, &msg, sizeof(msg));
  /* initialize return values */

  /* get replies */

  while (admwrk_get_bcast(ctx, &nodeid, EXAMSG_SERVICE_BARRIER, &rcv, sizeof(rcv), &err))
  {
    exalog_debug("%s: barrier from %s, step=%s, err=%d, rcv.err=%d (%s)",
		 adm_wt_get_name(), adm_cluster_get_node_by_id(nodeid)->name,
		 step, err, rcv.err, rcv.err != EXA_SUCCESS ? rcv.error_msg : "");

    if (err == -ADMIND_ERR_NODE_DOWN)
    {
      strlcpy(rcv.error_msg, exa_error_msg(err), sizeof(rcv.error_msg));
      ret = -ADMIND_ERR_NODE_DOWN;
    }
    else
    {
      err = rcv.err;
    }

    if (ret != -ADMIND_ERR_NODE_DOWN &&
	rcv.err != -ADMIND_ERR_NOTHINGTODO &&
	rcv.err != EXA_SUCCESS)
    {
      /* Don't hide a real error with a warning and an info. */
      if (get_error_type(-rcv.err) == ERR_TYPE_ERROR ||
          ret == EXA_SUCCESS)
        ret = rcv.err;

      adm_write_inprogress(adm_nodeid_to_name(nodeid),
	                   step, rcv.err, rcv.error_msg);
      sent_inprogress = true;
    }
  }

  if (!sent_inprogress)
    adm_write_inprogress(adm_nodeid_to_name(adm_myself()->id), step,
	                 EXA_SUCCESS, exa_error_msg(EXA_SUCCESS));

  return ret;
}

