/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md/src/md_event_list.h"
#include "monitoring/md/src/md_trap_sender.h"
#include "monitoring/md/src/md_srv_com.h"
#include "monitoring/agentx/src/md_handler.h"
#include "monitoring/agentx/src/md_heartbeat.h"
#include "monitoring/common/include/md_constants.h"

#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <unit_testing.h>
#include <stdbool.h>

extern void md_heartbeat_loop();


md_msg_agent_trap_t* incoming_traps;
int incoming_traps_count;
int incoming_traps_index;
bool suicided = false;

typedef enum {
    DISK_GROUP_STATUS_TRAP,
    DISK_STATUS_TRAP,
    FILESYSTEM_STATUS_TRAP,
    NODE_STATUS_TRAP
} fake_trap_type_t;


typedef struct {
    fake_trap_type_t type;
    char id[256];
} fake_trap_t;

fake_trap_t sent_traps[256];
int sent_traps_count;

/* fake */
typedef struct {
    u_char* buffer;
    int size;
} trap_var_t;
trap_var_t* trap_vars = NULL;

/* fake */
int snmp_log(int priority, const char *format, ...)
{
  va_list al;
  va_start(al, format);
  vprintf(format, al);
  va_end(al);
  printf("\n");
  return 0;
}

/* fake */
bool md_messaging_setup()
{
    incoming_traps = NULL;
    incoming_traps_index = 0;

    memset(&sent_traps, 0, sizeof(sent_traps));
    sent_traps_count = 0;

    return true;
}
void md_messaging_cleanup()
{
}

bool md_messaging_recv_event(md_msg_event_t *event)
{
    if (incoming_traps_index > incoming_traps_count - 1)
	return false;
    event->type = MD_MSG_EVENT_TRAP;
    event->content.trap = incoming_traps[incoming_traps_index++];
    return true;
}

/* fake */
int send_exaDiskGroupStatusNotification_trap()
{
    sent_traps[sent_traps_count].type = DISK_GROUP_STATUS_TRAP;
    strcpy(sent_traps[sent_traps_count].id, (char *)trap_vars[0].buffer);
    printf("Sent trap DISK_GROUP_STATUS_TRAP(%s)!\n", sent_traps[sent_traps_count].id);
    ++sent_traps_count;
    return 0;
}

int send_exaDiskStatusNotification_trap()
{
    sent_traps[sent_traps_count].type = DISK_STATUS_TRAP;
    strcpy(sent_traps[sent_traps_count].id, (char *)trap_vars[0].buffer);
    printf("Sent trap DISK_STATUS_TRAP(%s)!\n", sent_traps[sent_traps_count].id);
    ++sent_traps_count;
    return 0;
}

int send_exaFilesystemStatusNotification_trap()
{
    sent_traps[sent_traps_count].type = FILESYSTEM_STATUS_TRAP;
    strcpy(sent_traps[sent_traps_count].id, (char *)trap_vars[0].buffer);
    printf("Sent trap FILESYSTEM_STATUS_TRAP(%s)!\n", sent_traps[sent_traps_count].id);
    ++sent_traps_count;
    return 0;
}

int send_exaNodeStatusNotification_trap()
{
    sent_traps[sent_traps_count].type = NODE_STATUS_TRAP;
    strcpy(sent_traps[sent_traps_count].id, (char *)trap_vars[0].buffer);
    printf("Sent trap NODE_STATUS_TRAP(%s)!\n", sent_traps[sent_traps_count].id);
    ++sent_traps_count;
    return 0;
}



/* fake */
void commit_suicide()
{
    suicided = true;
}


/* TODO would be nice to have a setup "per test case" added to unit testing framework */
static void setup()
{
    suicided = false;
}




ut_test(test_alive_states)
{
    bool stop = false;
    os_thread_t srv_com_thread;
    os_thread_t handler_thread;

    setup();

    /* start srv_com and md_handler threads
     * to accept connections and answer alive messages */
    UT_ASSERT(os_thread_create(&srv_com_thread, 0,
                               md_srv_com_thread, NULL));
    /* let some time for threads to start and connect each other */
    os_sleep(1);

    UT_ASSERT(os_thread_create(&handler_thread, 0,
                               md_handler_thread, &stop));

    /* let some time for threads to start and connect each other */
    os_sleep(1);

    /* init state, everybody is considered not alive */
    UT_ASSERT(!md_srv_com_is_agentx_alive());
    UT_ASSERT(!md_is_alive());

    /* loop heartbeat to send an alive message */
    md_heartbeat_loop();
    os_millisleep(500);
    UT_ASSERT(md_srv_com_is_agentx_alive());
    UT_ASSERT(md_is_alive());

    /* no heartbeat from agentx for more than timeout seconds */
    os_sleep(MD_HEARTBEAT_TIMEOUT_SECONDS);

    UT_ASSERT(!md_srv_com_is_agentx_alive());

    /* heartbeat is back */
    md_heartbeat_loop();
    os_millisleep(500);
    UT_ASSERT(md_srv_com_is_agentx_alive());
    UT_ASSERT(md_is_alive());

    /* FIXME Threads handler_thread and srv_com_thread were canceled here.
     * Thread cancellation is now forbidden. The threads must be fixed. */
}


/* FIXME Added timeout because some threads aren't joinable */
ut_test(test_trap_sending_chain) __ut_timeout(60s)
{
    int t = 0;
    os_thread_t trap_sender_thread;
    os_thread_t srv_com_thread;
    os_thread_t event_list_thread;
    os_thread_t handler_thread;
    os_thread_t heartbeat_thread;
    bool stop = false;

    md_msg_agent_trap_t traps[4] =
	{
	    {
		.type = MD_NODE_TRAP,
		.id = "sam75",
		.status = 2
	    },
	    {
		.type = MD_DISK_TRAP,
		.id = "sam76",
		.status = 1
	    },
	    {
		.type = MD_FILESYSTEM_TRAP,
		.id = "sam77",
		.status = 0
	    },
	    {
		.type = MD_NODE_TRAP,
		.id = "sam78",
		.status = 2
	    }
	};
    setup();

    UT_ASSERT(md_messaging_setup());

    incoming_traps = traps;
    incoming_traps_count = 4;

    UT_ASSERT(os_thread_create(&srv_com_thread, 0,
			       md_srv_com_thread, NULL));
    /* let some time for thread to start and connect */
    os_sleep(1);
    UT_ASSERT(os_thread_create(&handler_thread, 0,
			       md_handler_thread, &stop));
    UT_ASSERT(os_thread_create(&heartbeat_thread, 0,
			       md_heartbeat_thread, &stop));

    UT_ASSERT(os_thread_create(&event_list_thread, 0,
			       md_event_list_thread, &stop));
    UT_ASSERT(os_thread_create(&trap_sender_thread, 0,
			       md_trap_sender_thread, &stop));
    os_sleep(1);

    UT_ASSERT(md_srv_com_is_agentx_alive());

    UT_ASSERT_EQUAL(4, sent_traps_count);

    UT_ASSERT_EQUAL(NODE_STATUS_TRAP, sent_traps[t].type);
    UT_ASSERT_EQUAL_STR("sam75", sent_traps[t].id);
    ++t;
    UT_ASSERT_EQUAL(DISK_STATUS_TRAP, sent_traps[t].type);
    UT_ASSERT_EQUAL_STR("sam76", sent_traps[t].id);
    ++t;
    UT_ASSERT_EQUAL(FILESYSTEM_STATUS_TRAP, sent_traps[t].type);
    UT_ASSERT_EQUAL_STR("sam77", sent_traps[t].id);
    ++t;
    UT_ASSERT_EQUAL(NODE_STATUS_TRAP, sent_traps[t].type);
    UT_ASSERT_EQUAL_STR("sam78", sent_traps[t].id);

    /* Stop & join all threads */
    stop = true;
    os_thread_join(trap_sender_thread);
    os_thread_join(event_list_thread);

    md_srv_com_thread_stop();
    os_thread_join(srv_com_thread);

    /* FIXME As of now heartbeat and handler can't be joined */
    os_thread_join(heartbeat_thread);
    os_thread_join(handler_thread);

    md_messaging_cleanup();

}
