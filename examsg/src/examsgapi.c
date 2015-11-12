/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <os/include/os_error.h>

#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"

#include "examsg/include/examsg.h"
#include "log/include/log.h"

#include "objpoolapi.h"
#include "mailbox.h"
#include "examsgd/network.h"

/* Exported shared predefined nodesets.
 * The "otherhosts" nodeset is initialized when examsgNetInit() is called
 * by examsgd, so that attempting to communicate using this nodeset before
 * examsgd is spawned will fail.
 */
const exa_nodeset_t *const EXAMSG_LOCALHOST = &EXA_NODESET_EMPTY;
const exa_nodeset_t *const EXAMSG_ALLHOSTS  = &EXA_NODESET_FULL;
/** Private structure owned by users of examsg */
struct ExamsgHandle
{
    ExamsgID owner;        /**< Owner id */
    mbox_set_t *mbox_set;  /**< Set of mboxes created with this handle */
};


static ExamsgHandle
examsgHandleAlloc(void)
{
  return (ExamsgHandle)os_malloc(sizeof(struct ExamsgHandle));
}

static void
examsgHandleFree(ExamsgHandle mh)
{
  os_free(mh);
}

int examsg_static_init(examsg_static_op_t op)
{
    EXA_ASSERT_VERBOSE(op == EXAMSG_STATIC_CREATE || op == EXAMSG_STATIC_GET,
                       "Invalid static init op: %d", op);

    if (op == EXAMSG_STATIC_CREATE)
	return examsgMboxCreateAll();
    else /* EXAMSG_STATIC_GET */
	return examsgMboxMapAll();
}

void examsg_static_clean(examsg_static_op_t op)
{
    EXA_ASSERT_VERBOSE(op == EXAMSG_STATIC_RELEASE || op == EXAMSG_STATIC_DELETE,
                       "Invalid static clean op: %d", op);

    if (op == EXAMSG_STATIC_DELETE)
        examsgMboxDeleteAll();
    else /* EXAMSG_STATIC_RELEASE */
        examsgMboxUnmapAll();
}

/**
 * Get the error string for a given Examsg error code.
 *
 * \param[in] err  Negative error code
 *
 * \return Examsg-specific error string, if applicable;
 *         a standard error message otherwise
 */
static const char *
examsgErrorStr(int err)
{
  const char *msg;

  EXA_ASSERT(err < 0);

  if (err == -ENXIO)
    msg = "Mailbox non-existent";
  else if (err == -ENOSPC)
    msg = "Mailbox full";
  else
    msg = strerror(-err);

  return msg;
}

ExamsgHandle __examsgInit(ExamsgID owner, const char *file, unsigned int line)
{
  ExamsgHandle mh;

  EXA_ASSERT(EXAMSG_ID_VALID(owner));

  mh = examsgHandleAlloc();
  if (!mh)
    {
      exalog_error("error during examsgInit: malloc failed\n");
      return NULL;
    }

  mh->owner = owner;

  mh->mbox_set = mboxset_alloc(file, line);
  if (!mh->mbox_set)
  {
      examsgHandleFree(mh);
      mh = NULL;
  }

  return mh;
}

int examsgExit(ExamsgHandle mh)
{
    if (!mh)
	return -EINVAL;

    mboxset_free(mh->mbox_set);
    examsgHandleFree(mh);

    return 0;
}

ExamsgID examsgOwner(ExamsgHandle mh)
{
  return mh->owner;
}

int examsgAddMbox(ExamsgHandle mh, ExamsgID id, size_t num_msg, size_t msg_size)
{
  int err;

  EXA_ASSERT(mh);
  EXA_ASSERT(num_msg > 0 && msg_size > 0);

    /* msg_size + sizeof(MID) because each message need a mid */
  err = examsgMboxCreate(mh->owner, id, num_msg,
                         msg_size + sizeof(ExamsgMID));
  if (!err)
      mboxset_add(mh->mbox_set, id);

  return err;
}

int examsgDelMbox(ExamsgHandle mh, ExamsgID id)
{
  int s;

  if (!mh)
    return -EINVAL;

  s = examsgMboxDelete(id);
  if (!s)
      mboxset_del(mh->mbox_set, id);

  return s;
}


/* --- __examsgSendWithFlag ---------------------------------------------------- */

/** \brief Send a message to a mailbox.
 *
 * The function sends a message to the mailbox of component \a to. The
 * message appears to come from the owner of \a mh.
 *
 * \param[in] mh	Examsg handle created by examsgInit().
 * \param[in] to	Recipient id.
 * \param[in] dest_nodes Destination nodes
 * \param[in] any	Examsg header to prepend (MUST NOT BE NULL).
 * \param[in] payload	Content of the message, may be NULL IIF nbytes == 0.
 * \param[in] nbytes	Length of the payload, in bytes.
 * \param[in] flags	Flags to use
 *
 * \return 0 or a negative error code
 */
static int
__examsgSendWithFlag(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
		     const ExamsgAny *any, const void *payload, size_t nbytes,
		     ExamsgFlags flags)
{
  int n;
  ExamsgMID mid;
  ExamsgNetRqst netheader;
  int idfrom, idto;
  char hex_nodes[EXA_NODESET_HEX_SIZE + 1];
  size_t net_payload;
  bool is_net_message;

  EXA_ASSERT(mh);
  EXA_ASSERT(EXAMSG_ID_VALID(to));
  EXA_ASSERT(dest_nodes);
  EXA_ASSERT(any);
  EXA_ASSERT(payload != NULL || nbytes == 0);

  is_net_message = !exa_nodeset_equals(dest_nodes, EXAMSG_LOCALHOST);

  if (is_net_message)
      exa_nodeset_to_hex(dest_nodes, hex_nodes);
  else
      strlcpy(hex_nodes, "LOCALHOST", sizeof(hex_nodes));

  idfrom = mh->owner;
  idto = to;

  net_payload = nbytes + (any ? sizeof(*any) : 0);
  if (net_payload > sizeof(Examsg))
    return -EMSGSIZE;

  exalog_trace("sending %" PRIzu " bytes to `%s'@`%s'",
	       net_payload, examsgIdToName(to), hex_nodes);

  /* setup message id */
  memset(mid.host, 0, sizeof(mid.host)); /* localhost */
  uuid_zero(&mid.netid.cluster);
  mid.netid.node = EXA_NODEID_LOCALHOST;
  mid.id = idfrom;

  if (is_net_message)
    {
      netheader.to = idto;
      netheader.flags = flags;
      netheader.size = net_payload;
      exa_nodeset_copy(&netheader.dest_nodes, dest_nodes);
      idto = EXAMSG_NETMBOX_ID;
    }

  /* if it is a network message, we have to tack on the network header */
  if (is_net_message)
      n = __examsgMboxSend(idfrom, idto, flags,
	      &mid,                     sizeof(mid),
	      (const char *)&netheader, sizeof(netheader),
	      (const char *)any,        sizeof(*any),
	      (const char *)payload,    nbytes,
	      NULL, 0);
  else
      n = __examsgMboxSend(idfrom, idto, flags,
	      &mid,                  sizeof(mid),
	      (const char *)any,     sizeof(*any),
	      (const char *)payload, nbytes,
	      NULL, 0);

  if (n < 0)
      exalog_debug("sending to `%s'@`%s': %s",
                   examsgIdToName(to), hex_nodes, examsgErrorStr(n));
  return n < 0 ? n : 0;
}

/**
 * Wrapper around __examsgSendWithFlag just to keep former behaviour
 * (ie returning the size of data sent in case success, and maybe pass any == NULL)
 */
static int
examsgSendWithFlag(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
		     const ExamsgAny *_any, const void *_buffer, size_t _nbytes,
		     ExamsgFlags flags)
{
  int ret;
  /* The _any field may not be provided. In that case, the _buffer MUST be of
   * Examsg type and thus MUST have its own header ExamsgAny. So here if _any
   * is NULL, we get the header in the buffer (where is actually is); if _any
   * if provided we use it directly. As a matter of fact, in case buffer
   * contains the header, the payload is at any->payload address. */
  const ExamsgAny *any = _any ? _any : _buffer;
  const void *payload = _any ? _buffer : ((Examsg *)any)->payload;
  size_t nbytes = _any ? _nbytes : _nbytes - sizeof(*any);

  ret = __examsgSendWithFlag(mh, to, dest_nodes, any, payload, nbytes, flags);
  return ret == 0 ? _nbytes : ret;
}

int examsgSend(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
               const void *buffer, size_t nbytes)
{
  return examsgSendWithFlag(mh, to, dest_nodes, NULL, buffer, nbytes,
			   EXAMSGF_NONE);
}

int examsgSendWithHeader(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
		     const struct ExamsgAny *any,
		     const void *buffer, size_t nbytes)
{
  return examsgSendWithFlag(mh, to, dest_nodes, any, buffer, nbytes,
			    EXAMSGF_NONE);
}

int examsgSendNoBlock(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
		      const void *buffer, size_t nbytes)
{
  return examsgSendWithFlag(mh, to, dest_nodes, NULL, buffer, nbytes,
			    EXAMSGF_NOBLOCK);
}

int examsgRecv(ExamsgHandle mh, ExamsgMID *mid, void *buffer, size_t maxbytes)
{
  ExamsgMID lmid;
  int n;
  int id = mh->owner;

  n = examsgMboxRecv(id, &lmid, sizeof(lmid), buffer, maxbytes);
  if (n < 0)
    {
      if (mid)
	*mid = lmid;

      if (mh->owner != EXAMSG_LOGD_ID)
	exalog_error("cannot receive message: error %d", -n);

      return n;
    }

  if (!n)
    return 0;

  if (mid)
    *mid = lmid;

  return n - sizeof(lmid);
}


/** Acknowledgement message */
typedef struct {
    ExamsgAny ack; /**< type and id of acknowledged message */
    int error;     /**< 0 on success */
} examsg_ack_t;


int examsgAckReply(ExamsgHandle mh, const Examsg *msg, int error, ExamsgID to,
	           const exa_nodeset_t *dest_nodes)
{
  ExamsgAny header;
  examsg_ack_t ack;
  int s;

  EXA_ASSERT(exa_nodeset_equals(dest_nodes, EXAMSG_LOCALHOST)
	     || exa_nodeset_count(dest_nodes) == 1);

  /* build message structure */
  header.type = EXAMSG_ACK;
  ack.ack = msg->any;
  ack.error = error;

  s = examsgSendWithHeader(mh, to, dest_nodes, &header, &ack, sizeof(ack));
  if (s != sizeof(ack))
    return s;

  return 0;
}

int examsgSendWithAck(ExamsgHandle mh, ExamsgID to,
	              const exa_nodeset_t *dest_nodes,
		      const Examsg *msg, size_t nbytes,
		      int *ackError)
{
  Examsg reply;
  examsg_ack_t *ack = (examsg_ack_t *)reply.payload;
  int s;

  s = examsgSend(mh, to, dest_nodes, msg, nbytes);
  if (s != nbytes)
    return s;

  /* wait for ack */
  do {
    s = examsgWait(mh);
    if (s < 0)
      return s;

    s = examsgRecv(mh, NULL, &reply, sizeof(reply));
  } while (s == 0);

  if (s < 0)
    return s;

  EXA_ASSERT_VERBOSE(reply.any.type == EXAMSG_ACK
		     && ack->ack.type == msg->any.type,
		     "Bad Ack type='%d' atype='%d'",
		     reply.any.type, ack->ack.type);

  *ackError = ack->error;

  return nbytes;
}

int examsgWaitInterruptible(ExamsgHandle mh, struct timeval *timeout)
{
  return examsgMboxWait(mh->owner, mh->mbox_set, timeout);
}

int examsgWaitTimeout(ExamsgHandle mh, struct timeval *timeout)
{
  int s;

  do
      s = examsgWaitInterruptible(mh, timeout);
  while (s == -EINTR);

  return s;
}

int examsgWait(ExamsgHandle mh)
{
  return examsgWaitTimeout(mh, NULL);
}

void examsg_show_stats(void)
{
    examsgMboxMapAll();
    examsgMboxShowStats();
    examsgMboxUnmapAll();
}


