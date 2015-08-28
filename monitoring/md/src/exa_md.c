/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md/src/md_trap_sender.h"
#include "monitoring/md/src/md_event_list.h"
#include "monitoring/md/src/md_srv_com.h"
#include "monitoring/md/src/md_controller.h"
#include "monitoring/common/include/md_messaging.h"

#include "common/include/threadonize.h"
#include "common/include/exa_names.h"
#include "common/include/exa_error.h"

#include "examsg/include/examsg.h"

#include "log/include/log.h"

#include "os/include/os_time.h"
#include "os/include/os_daemon_child.h"

#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>



/** Whether the daemon should quit */
static bool stop;


/* Signal handler */
static void
signal_handler(int sig)
{
  stop = true;
}

static void init_md_controller(os_thread_t* thread)
{
    if (!exathread_create_named(thread, 0, md_controller_thread,
		                (void *)&stop, "exa_md_controller_thread"))
	exalog_error("Failed to create md_controller thread.");
}

static void clean_md_controller(os_thread_t thread)
{
    /* The thread stops when stop boolean is set */
    os_thread_join(thread);
}

static void init_md_srv_com(os_thread_t* thread)
{
    md_srv_com_static_init();
    if (!exathread_create_named(thread, 0, md_srv_com_thread,
                                NULL /* has its own stop callback */,
				"exa_md_srv_com_thread"))
	exalog_error("Failed to create md_srv_com thread.");
}

static void clean_md_srv_com(os_thread_t thread)
{
    /* The thread stops when stop boolean is set */
    os_thread_join(thread);
    md_srv_com_static_clean();
}

static void init_md_trap_sender(os_thread_t* thread)
{
    md_trap_sender_static_init();
    if (!exathread_create_named(thread, 0, md_trap_sender_thread,
		                (void *)&stop, "exa_md_trap_sender_thread"))
	exalog_error("Failed to create md_trap_sender thread.");
}

static void clean_md_trap_sender(os_thread_t thread)
{
    /* The thread stops when stop boolean is set */
    os_thread_join(thread);
    md_trap_sender_static_clean();
}

static void init_md_event_list(os_thread_t* thread)
{
    if (!exathread_create_named(thread, 0, md_event_list_thread,
		                (void *)&stop, "exa_md_event_list_thread"))
	exalog_error("Failed to create md_event_list thread.");
}

static void clean_md_event_list_thread(os_thread_t thread)
{
    /* The thread stops when stop boolean is set */
    os_thread_join(thread);
}

int daemon_init(int argc,  char *argv[])
{
    int ret;

    exalog_as(EXAMSG_MONITORD_CONTROL_ID);

    ret = examsg_static_init(EXAMSG_STATIC_GET);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Can't initialize messaging layer.");
	return ret;
    }

    exalog_static_init();

    if (!md_messaging_setup())
    {
        exalog_error("Could not setup md messaging!");
        md_messaging_cleanup();
        return -ENOMEM;
    }

    /* Ready */
    exalog_info("exa_md daemonized");

    return EXA_SUCCESS;
}

int daemon_main(void)
{
    os_thread_t trap_sender_thread;
    os_thread_t srv_com_thread;
    os_thread_t event_list_thread;
    os_thread_t controller_thread;
    bool agentx_was_up = false;

    /* register the signal handler in order to be able to stop properly */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    /* Make sure threads wont imediatly stop */
    stop = false;

    init_md_controller(&controller_thread);
    init_md_srv_com(&srv_com_thread);
    init_md_trap_sender(&trap_sender_thread);
    init_md_event_list(&event_list_thread);

    while (!stop)
    {
        if (daemon_must_quit())
        {
            stop = true;
            continue;
        }

        os_sleep(1);
        if (!md_srv_com_is_agentx_alive())
        {
            if (agentx_was_up)
            {
                exalog_warning("exa_agentx seems to be down!");
            }
            agentx_was_up = false;
        }
        else
        {
            agentx_was_up = true;
        }
    }

    md_srv_com_thread_stop();

    clean_md_event_list_thread(event_list_thread);
    clean_md_trap_sender(trap_sender_thread);
    clean_md_srv_com(srv_com_thread);
    clean_md_controller(controller_thread);

    md_messaging_cleanup();

    exalog_debug("exa_md finished.");

    exalog_static_clean();

    examsg_static_clean(EXAMSG_STATIC_RELEASE);

    exit(0);
}




