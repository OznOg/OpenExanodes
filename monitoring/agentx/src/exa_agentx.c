/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/snmp_logging.h>
#include <signal.h>

#include "common/include/exa_names.h"
#include "common/include/uuid.h"
#include "common/include/exa_error.h"

#include "monitoring/common/include/md_constants.h"
#include "monitoring/agentx/src/md_heartbeat.h"
#include "monitoring/agentx/src/md_handler.h"
#include "monitoring/agentx/src/mib_notify.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_stdio.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <errno.h>
#include <unistd.h>


static bool stop = false;

static exa_nodeid_t node_id;
static char *node_name;



static RETSIGTYPE stop_server(int a)
{
    stop = true;
}

static void init_snmp_subagent(const char *agentx_host, int agentx_port)
{
    char agentx_socket[128];
    int background = 1; /* change this if you want to run in the background */
    int syslog = 0; /* change this if you want to use syslog */

    snmp_enable_filelog("/var/log/exa_agentx.log", 1);

    snmp_log(LOG_INFO, "exa_agentx configured to connect to %s:%d\n",
	     agentx_host, agentx_port);

    /* we're in an agentx subagent */
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);
    sprintf(agentx_socket, "tcp:%s:%d", agentx_host, agentx_port);
    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
                          NETSNMP_DS_AGENT_X_SOCKET, agentx_socket);

    /* run in background, if requested */
    if (background && netsnmp_daemonize(1, !syslog))
        exit(1);

    snmp_log(LOG_INFO, "exa_agentx daemonized\n");

    /* initialize tcpip, if necessary */
    SOCK_STARTUP;

    /* initialize the agent library */
    init_agent("exa_agentx");

    /* initialize mib code here */

    /* mib code: init_nstAgentSubagentObject from nstAgentSubagentObject.C */
    /* not necessary to just send TRAPs !! */
/*   init_mib_int_watch(); */

    /* example-demon will be used to read example-demon.conf files. */
    init_snmp("exa_agentx");

    /* In case we recevie a request to stop (kill -TERM or kill -INT) */
    stop = false;
    signal(SIGTERM, stop_server);
    signal(SIGINT, stop_server);

    snmp_log(LOG_INFO, "exa_agentx is up and running.\n");
}


static void init_md_handler(void)
{
    os_thread_t thread;

    if (!os_thread_create(&thread, 0, md_handler_thread, &stop))
        snmp_log(LOG_ERR, "Could not create md_handler thread.\n");
}


static void init_md_heartbeat(void)
{
    os_thread_t thread;

    if (!os_thread_create(&thread, 0, md_heartbeat_thread, &stop))
        snmp_log(LOG_ERR, "Could not create md_heartbeat thread.\n");
}


void commit_suicide(void)
{
    md_msg_agent_trap_t farewell_trap;
    farewell_trap.type = MD_NODE_TRAP;
    os_snprintf(farewell_trap.id, sizeof(exa_uuid_str_t),
                "%08X:%08X:%08X:%08X",
                0, 0, 0, node_id);
    os_snprintf(farewell_trap.name, sizeof(farewell_trap.name), "%s", node_name);
    farewell_trap.status = MD_NODE_STATUS_DOWN;
    md_send_trap(&farewell_trap);
    kill(getpid(), SIGTERM);
}


/* FIXME Returning EINVAL is not right */
int main(int argc,  char *argv[])
{
    const char *master_agentx_host = NULL;
    uint32_t master_agentx_port = 0;

    /* agentx host should always be provided */
    if (argc < 4)
        return EINVAL;

    if (sscanf(argv[1], "%u", &node_id) != 1)
        return EINVAL;
    node_name = argv[2];
    master_agentx_host = argv[3];

    /* The port parameter is optional: 0 is a valid value for agentx port
       (means default port) */
    if (argc >= 5)
	master_agentx_port = atoi(argv[4]);

    if (master_agentx_port == 0)
	master_agentx_port = MD_DEFAULT_MASTER_AGENTX_PORT;

    init_snmp_subagent(master_agentx_host,
		       master_agentx_port);
    init_md_handler();
    init_md_heartbeat();

    while (!stop)
    {
        os_sleep(1);
        agent_check_and_process(0); /* 0 == don't block */
    }

    snmp_log(LOG_INFO, "exit exa_agentx.\n");
    snmp_shutdown("exa_agentx");
    SOCK_CLEANUP;

    return EXA_SUCCESS;
}
