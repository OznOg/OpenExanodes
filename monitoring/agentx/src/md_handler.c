/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/agentx/src/md_handler.h"
#include "monitoring/agentx/src/md_heartbeat.h"
#include "monitoring/common/include/md_constants.h"
#include "monitoring/common/include/md_types.h"
#include "monitoring/agentx/src/mib_notify.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <stdbool.h>
#include <assert.h>
#include <unistd.h>

/**
 * Global variable holding trap payload variables.
 * This should be perfectly safe since md_handler thread must be
 * the only one to use send traps with net-snmp.
 */
extern trap_var_t *trap_vars;


/** md_com connection with monitoring daemon;
 *  only md_handler thread is receiving but sending is allowed from
 *  other threads.
 */
int md_connection = -1;
os_thread_mutex_t md_connection_mutex;


void md_send_trap(const md_msg_agent_trap_t *trap)
{
    trap_var_t vars[] = {
        {
            .buffer = (u_char *) trap->id,
            .size  = strlen(trap->id)
        },
        {
            .buffer = (u_char *) trap->name,
            .size  = strlen(trap->name)
        },
        {
            .buffer = (u_char *) &trap->status,
            .size  = sizeof(trap->status)
        },
        { NULL, 0 }
    };
    snmp_log(LOG_INFO, "Sending trap id=%s name=%s status=%d.\n",
             trap->id, trap->name, trap->status);

    trap_vars = vars;

    switch (trap->type)
    {
    case MD_NODE_TRAP:
        send_exaNodeStatusNotification_trap();
        break;

    case MD_DISK_TRAP:
        send_exaDiskStatusNotification_trap();
        break;

    case MD_DISKGROUP_TRAP:
        send_exaDiskGroupStatusNotification_trap();
        break;

    case MD_VOLUME_TRAP:
/* 	send_exaVolumeNotification_trap(); */
        break;

    case MD_FILESYSTEM_TRAP:
        send_exaFilesystemStatusNotification_trap();
        break;

    default:
        snmp_log(LOG_ERR, "Unknown trap type.\n");
    }
}

static void handle_trap_msg(md_com_msg_t *msg)
{
    md_send_trap((md_msg_agent_trap_t *) msg->payload);
}

static void handle_req_msg(md_com_msg_t *msg)
{
    /* TODO */
}

static void handle_alive_msg(md_com_msg_t *msg)
{
    md_received_alive();
}

static void handle_unknown_msg(md_com_msg_t *msg)
{
    snmp_log(LOG_ERR, "Unknown message received from md.\n");
}

static void close_connection(void)
{
    md_com_error_code_t ret;
    os_thread_mutex_lock(&md_connection_mutex);
    ret = md_com_close(md_connection);
    if (ret != COM_SUCCESS)
        snmp_log(LOG_ERR, "Could not close connection with md.\n");
    md_connection = -1;
    os_thread_mutex_unlock(&md_connection_mutex);
}

static void check_connection(void)
{
    md_com_error_code_t ret;
    os_thread_mutex_lock(&md_connection_mutex);
    if (md_connection == -1)
    {
        snmp_log(LOG_INFO, "Connecting to md...\n");
        ret = md_com_connect(MD_COM_SOCKET_PATH, &md_connection);
        if (ret != COM_SUCCESS)
        {
            snmp_log(LOG_ERR, "agentx cannot connect to monitoring daemon.\n");
            os_thread_mutex_unlock(&md_connection_mutex);
            return;
        }
        snmp_log(LOG_INFO, "Connected to md.\n");
    }
    os_thread_mutex_unlock(&md_connection_mutex);
}

static bool is_connected(void)
{
    int tmp;
    os_thread_mutex_lock(&md_connection_mutex);
    tmp = md_connection;
    os_thread_mutex_unlock(&md_connection_mutex);
    return tmp != -1;
}

static void md_handler_loop(void)
{
    int ret;
    md_com_msg_t *rx_msg;

    check_connection();
    if (!is_connected())
    {
        os_sleep(1);
        return;
    }

    while (true)
    {
        rx_msg = (md_com_msg_t *) md_com_msg_alloc_rx();
        if (rx_msg == NULL)
            snmp_log(LOG_INFO, "Could not allocate rx message.\n");

        /* no mutex here, should be allowed to recv/send simultaneously */
        ret = md_com_recv_msg(md_connection, rx_msg);

        if (ret != COM_SUCCESS)
        {
            snmp_log(LOG_INFO, "Could not receive message from md.\n");
            md_com_msg_free_message(rx_msg);
            close_connection();
            break;
        }
        switch (rx_msg->type)
        {
        case MD_MSG_AGENT_TRAP:
            handle_trap_msg(rx_msg);
            break;

        case MD_MSG_AGENT_REQ:
            handle_req_msg(rx_msg);
            break;

        case MD_MSG_AGENT_ALIVE:
            handle_alive_msg(rx_msg);
            break;

        default:
            handle_unknown_msg(rx_msg);
        }

        md_com_msg_free_message(rx_msg);
    }
}

void md_handler_thread(void *pstop)
{
    bool *stop = pstop;
    while (!*stop)
    {
        md_handler_loop();
    }
}

void md_send_msg(const md_com_msg_t *tx_msg)
{
    if (!is_connected())
        return;
    os_thread_mutex_lock(&md_connection_mutex);
    md_com_send_msg(md_connection, tx_msg);
    os_thread_mutex_unlock(&md_connection_mutex);
}
