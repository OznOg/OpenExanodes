/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "monitoring/md/src/md_event_list.h"
#include "monitoring/common/include/md_messaging.h"
#include "monitoring/md/src/md_trap_sender.h"
#include "log/include/log.h"

static void handle_trap_event(md_msg_event_trap_t *trap)
{
    md_trap_sender_error_code_t err;
    /* dispatch trap event to trap sender */
    err = md_trap_sender_enqueue(trap);
    if (err == MD_TRAP_SENDER_OVERFLOW)
    {
	exalog_error("Failed to enqueue trap (overflow).");
    }
}

static void handle_endofcommand_event(md_msg_event_endofcommand_t* endofcommand)
{
    /* obsoletes concerned cache entries */
    /* TODO */
}

void md_event_list_loop(void)
{
    md_msg_event_t event;
    if (md_messaging_recv_event(&event))
    {
	switch(event.type)
	{
	case MD_MSG_EVENT_TRAP:
	    handle_trap_event(&event.content.trap);
	    break;
	case MD_MSG_EVENT_ENDOFCOMMAND:
	    handle_endofcommand_event(&event.content.endofcommand);
	    break;
	default:
	    exalog_error("Unknown event message type.");
	}
    }
}

void md_event_list_thread(void *pstop)
{
    bool *stop = (bool *)pstop;
    while (!*stop)
    {
	md_event_list_loop();
    }
}

