
/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "monitoring/md_client/include/md_control.h"
#include "monitoring/common/include/md_types.h"
#include "log/include/log.h"
#include "os/include/strlcpy.h"


EXAMSG_DCLMSG(md_examsg_control_t, md_msg_control_t control);
EXAMSG_DCLMSG(md_examsg_event_t, md_msg_event_t event);
EXAMSG_DCLMSG(md_examsg_control_status_reply_t, md_service_status_t status);



exa_error_code md_client_control_start(ExamsgHandle local_mh,
				       exa_nodeid_t node_id,
				       const char *node_name,
				       const char *master_agentx_host,
				       uint32_t master_agentx_port)
{
    int ret;
    int ack;
    md_examsg_control_t msg;
    msg.any.type = EXAMSG_MD_CONTROL;
    msg.control.type = MD_MSG_CONTROL_START;
    msg.control.content.start.node_id = node_id;
    strlcpy(msg.control.content.start.node_name,
	    node_name,
	    sizeof(msg.control.content.start.node_name));
    strlcpy(msg.control.content.start.master_agentx_host,
	    master_agentx_host,
	    sizeof(msg.control.content.start.master_agentx_host));
    msg.control.content.start.master_agentx_port = master_agentx_port;

    ret = examsgSendWithAck(local_mh, EXAMSG_MONITORD_CONTROL_ID, EXAMSG_LOCALHOST,
			    (const Examsg *)&msg, sizeof(msg), &ack);

    if (ret != sizeof(msg))
    {
	exalog_error("Cannot send start control message to monitoring daemon.");
	return -MD_ERR_START;
    }
    return ack;
}


exa_error_code md_client_control_stop(ExamsgHandle local_mh)
{
    int ret;
    int ack;
    md_examsg_control_t msg;
    msg.any.type = EXAMSG_MD_CONTROL;
    msg.control.type = MD_MSG_CONTROL_STOP;

    ret = examsgSendWithAck(local_mh, EXAMSG_MONITORD_CONTROL_ID, EXAMSG_LOCALHOST,
			    (const Examsg *)&msg, sizeof(msg), &ack);

    if (ret != sizeof(msg))
    {
	exalog_error("Cannot send stop control message to monitoring daemon.");
	return -MD_ERR_STOP;
    }
    return ack;
}


exa_error_code md_client_control_status(ExamsgHandle local_mh, md_service_status_t *status)
{
    int ret, s;
    md_examsg_control_t msg;
    md_examsg_control_status_reply_t reply;

    msg.any.type = EXAMSG_MD_CONTROL;
    msg.control.type = MD_MSG_CONTROL_STATUS;

    ret = examsgSend(local_mh, EXAMSG_MONITORD_CONTROL_ID, EXAMSG_LOCALHOST,
		     &msg, sizeof(msg));
    if (ret != sizeof(msg))
    {
	exalog_error("Cannot send status control message to monitoring daemon.");
	return -MD_ERR_STATUS;
    }

    /* wait for answer */
    do {
	s = examsgWait(local_mh);
	if (s < 0)
	    return s;

	s = examsgRecv(local_mh, NULL, &reply, sizeof(reply));
    } while (s == 0);

    if (s < 0)
	return s;

    *status = reply.status;
    return EXA_SUCCESS;
}



