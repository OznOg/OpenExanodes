/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "monitoring/common/include/md_messaging.h"
#include "monitoring/common/include/md_constants.h"
#include "monitoring/md/src/md_controller.h"
#include "monitoring/md/src/md_trap_sender.h"
#include "monitoring/md/src/md_srv_com.h"
#include "log/include/log.h"
#include "common/include/exa_system.h"
#include "common/include/exa_names.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "os/include/os_daemon_child.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"

#include <unistd.h>


static void handle_start_control(md_msg_control_start_t *start)
{
    int ret;

    if (!md_srv_com_is_agentx_alive())
    {
	char node_id_str[32];
	char master_agentx_port_str[EXA_MAXSIZE_LINE + 1];
	os_snprintf(node_id_str, sizeof(node_id_str), "%u", start->node_id);
	os_snprintf(master_agentx_port_str, sizeof(master_agentx_port_str), "%d",
                    start->master_agentx_port);

        /* XXX No use for a block here */
	{
            char agentx_path[OS_PATH_MAX];
	    char *const argv[] =
		{
                    agentx_path,
		    node_id_str,
		    start->node_name,
		    start->master_agentx_host,
		    master_agentx_port_str,
		    NULL
		};

            exa_env_make_path(agentx_path, sizeof(agentx_path), exa_env_sbindir(), "exa_agentx");
	    ret = exa_system(argv);
	}
	if (ret != EXA_SUCCESS)
	{
	    exalog_error("Error spawning exa_agentx(%d).", ret);
	    md_messaging_ack_control(ret);
	    return;
	}

	/* wait a bit to get the first alive message from agentx
	 * TODO : not robust enough, replace with a timeouted lock */
	os_sleep(MD_HEARTBEAT_TIMEOUT_SECONDS);

	if (!md_srv_com_is_agentx_alive())
	{
	    exalog_error("Error spawning exa_agentx(%d).", -MD_ERR_AGENTX_NOT_ALIVE);
	    md_messaging_ack_control(-MD_ERR_AGENTX_NOT_ALIVE);
	    return;
	}
    }
    md_messaging_ack_control(EXA_SUCCESS);
}

static void handle_stop_control(md_msg_control_stop_t *stop)
{
    int ret;
    char *const argv[] =
	{
	    "pkill",
            "exa_agentx",
	    NULL
	};
    ret = exa_system(argv);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Error stopping exa_agentx(%d).", ret);
	md_messaging_ack_control(ret);
	return;
    }
    md_messaging_ack_control(EXA_SUCCESS);
}

static void handle_status_control(md_msg_control_status_t *status)
{
    md_service_status_t service_status = MD_SERVICE_STOPPED;
    if (md_srv_com_is_agentx_alive())
    {
	service_status = MD_SERVICE_STARTED;
    }
    md_messaging_reply_control_status(service_status);
}

static void md_controller_loop(void)
{
    md_msg_control_t control;
    if (md_messaging_recv_control(&control))
    {
	switch(control.type)
	{
	case MD_MSG_CONTROL_START:
	    handle_start_control(&control.content.start);
	    break;
	case MD_MSG_CONTROL_STOP:
	    handle_stop_control(&control.content.stop);
	    break;
	case MD_MSG_CONTROL_STATUS:
	    handle_status_control(&control.content.status);
	    break;
	default:
	    exalog_error("Unknown event message type.");
	}
    }
}

void md_controller_thread(void *pstop)
{
    bool *stop = (bool *)pstop;
    while (!*stop)
    {
	md_controller_loop();
    }
}

