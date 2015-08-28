/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/common/include/md_messaging.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"

#include <assert.h>
#include <string.h>
#include <errno.h>


static ExamsgHandle mh_control;
static ExamsgHandle mh_event;


EXAMSG_DCLMSG(md_examsg_control_t, md_msg_control_t control);
EXAMSG_DCLMSG(md_examsg_event_t, md_msg_event_t event);
EXAMSG_DCLMSG(md_examsg_control_status_reply_t, md_service_status_t status);


/* this is a dirty hack in order to be able to acknowledge/answer
 * certain messages through examsg without breaking messaging abstraction
 * (we want to hide examsg from interface for sake of unit testing).
 * this method is valid given the fact that only md_controller consumes
 * control messages.
 */
md_examsg_control_t last_consumed_control_msg;
ExamsgMID last_consumed_control_msg_from;



static bool md_messaging_setup_mbox(ExamsgID examsg_id, ExamsgHandle *mh,
				    uint32_t nb_msg, uint32_t msg_size)
{
    int ret;

    EXA_ASSERT(mh != NULL);

    *mh = examsgInit(examsg_id);
    if (!*mh)
    {
	exalog_error("Can't run examsgInit()");
	return false;
    }

    ret = examsgAddMbox(*mh, examsgOwner(*mh), nb_msg, msg_size);
    if (ret)
    {
	exalog_error("cannot create md mailbox %d : %s (%d)",
		     examsg_id, exa_error_msg(ret), ret);
	if (*mh)
	    examsgExit(*mh);
      return false;
    }
    return true;
}

bool md_messaging_setup()
{
    return
	md_messaging_setup_mbox(EXAMSG_MONITORD_EVENT_ID, &mh_event,
				NBMAX_DISKS, sizeof(md_msg_event_t)) &&
	md_messaging_setup_mbox(EXAMSG_MONITORD_CONTROL_ID, &mh_control,
				3, sizeof(md_msg_control_t));
}



void md_messaging_cleanup()
{
    examsgDelMbox(mh_event, EXAMSG_MONITORD_EVENT_ID);
    examsgDelMbox(mh_control, EXAMSG_MONITORD_CONTROL_ID);
    examsgExit(mh_event);
    examsgExit(mh_control);
    mh_event = NULL;
    mh_control = NULL;
}

static bool md_messaging_recv(ExamsgHandle *mh, ExamsgMID *from,
			      void *msg, int msg_size)
{
    int ret;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 500000};

    /* wait for a message or a signal
     * Wake up every 'timeout' in order to see when the thread is requested
     * to stop */
    ret = examsgWaitInterruptible(*mh, &timeout);
    if (ret == -EINTR)
	return false;

    if (ret != 0)
	return false;

    memset(msg, 0, msg_size);
    if (from != NULL)
	memset(from, 0, sizeof(ExamsgMID));

    ret = examsgRecv(*mh, from, msg, msg_size);

    /* Error while message reception */
    if (ret < 0)
    {
	exalog_error("encountered an error while retrieving message : %s (%d)",
		     exa_error_msg(ret), ret);
	return false;
    }

    /* No message */
    if (ret == 0)
	return false;

    assert(ret == msg_size);
    return true;
}




bool md_messaging_recv_control(md_msg_control_t *control)
{
    md_examsg_control_t msg;
    int ret;

    ret = md_messaging_recv(&mh_control, &last_consumed_control_msg_from,
			    &msg, sizeof(msg));
    if (!ret)
	return false;

    assert(msg.any.type == EXAMSG_MD_CONTROL);
    *control = msg.control;

    last_consumed_control_msg = msg;
    return true;
}



bool md_messaging_ack_control(int error)
{
    int ret;
    ret = examsgAckReply(mh_control, (Examsg *)&last_consumed_control_msg,
			 error, last_consumed_control_msg_from.id,
			 EXAMSG_LOCALHOST);
    return (ret == 0);
}



bool md_messaging_reply_control_status(md_service_status_t status)
{
    int ret;
    md_examsg_control_status_reply_t msg;
    msg.any.type = EXAMSG_MD_STATUS;
    msg.status = status;

    ret = examsgSend(mh_control, last_consumed_control_msg_from.id,
		     EXAMSG_LOCALHOST, &msg, sizeof(msg));
    return (ret == sizeof(msg));
}




bool md_messaging_recv_event(md_msg_event_t *event)
{
    md_examsg_event_t msg;
    int ret;

    ret = md_messaging_recv(&mh_event, NULL, &msg, sizeof(msg));
    if (!ret)
	return false;

    assert(msg.any.type == EXAMSG_MD_EVENT);
    *event = msg.event;
    return true;
}

