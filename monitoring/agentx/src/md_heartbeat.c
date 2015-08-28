/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/agentx/src/md_heartbeat.h"
#include "monitoring/agentx/src/md_handler.h"
#include "monitoring/md_com/include/md_com.h"
#include "monitoring/common/include/md_types.h"
#include "monitoring/common/include/md_constants.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <unistd.h>

static struct timespec alive_msg_time;
static os_thread_mutex_t alive_msg_time_mutex = PTHREAD_MUTEX_INITIALIZER;


static void reset_alive_msg_time(void)
{
    os_thread_mutex_lock(&alive_msg_time_mutex);
    os_get_monotonic_time(&alive_msg_time);
    os_thread_mutex_unlock(&alive_msg_time_mutex);
}

extern void commit_suicide(void);

bool md_is_alive(void)
{
    struct timespec now;
    time_t elapsed;
    os_get_monotonic_time(&now);
    os_thread_mutex_lock(&alive_msg_time_mutex);
    elapsed = difftime(now.tv_sec, alive_msg_time.tv_sec);
    os_thread_mutex_unlock(&alive_msg_time_mutex);
    return elapsed < MD_HEARTBEAT_TIMEOUT_SECONDS;
}

void md_received_alive(void)
{
    reset_alive_msg_time();
}

void md_heartbeat_loop(void)
{
    static bool init = true;
    md_com_msg_t *tx_msg;
    md_msg_agent_alive_t alive;

    /* if we did not received answer alive message before
     * heartbeat timeout, the monitoring daemon is considered dead;
     * => suicide */
    if (!init && !md_is_alive())
    {
	commit_suicide();
    }

    /* send alive message */
    tx_msg = md_com_msg_alloc_tx(MD_MSG_AGENT_ALIVE,
				 (const char*) &alive,
				 sizeof(md_msg_agent_alive_t));
    md_send_msg(tx_msg);
    md_com_msg_free_message(tx_msg);

    os_sleep(MD_HEARTBEAT_PERIOD_SECONDS);
    init = false;

}

void md_heartbeat_thread(void *pstop)
{
    bool *stop = pstop;
    while (!*stop)
    {
	md_heartbeat_loop();
    }
}

