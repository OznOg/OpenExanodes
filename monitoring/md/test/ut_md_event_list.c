/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md/src/md_event_list.h"
#include "monitoring/md/src/md_trap_sender.h"
#include "monitoring/md_com/include/md_com_msg.h"
#include "os/include/os_thread.h"

#include <unit_testing.h>
#include <stdbool.h>


extern void md_event_list_loop();
extern void md_trap_sender_loop();
extern bool md_trap_sender_queue_empty();


md_msg_agent_trap_t* incoming_traps;
int incoming_traps_count;
int incoming_traps_index;

md_msg_agent_trap_t sent_traps[4096];
int sent_traps_count;



ut_setup()
{
    memset(&sent_traps, 0, sizeof(sent_traps));
    sent_traps_count = 0;
}


ut_cleanup()
{

}


/* fake com implementation for ut purpose */
void md_srv_com_send_msg(const md_com_msg_t* tx_msg)
{
    md_msg_agent_trap_t *payload =
	(md_msg_agent_trap_t*) tx_msg->payload;

    sent_traps[sent_traps_count++] = *(payload);
}

/* fake implementation for ut purpose */
bool md_srv_com_is_agentx_alive()
{
    return true;
}



/* fake messaging implementation for ut purpose */
bool md_messaging_setup()
{
    incoming_traps = NULL;
    incoming_traps_index = 0;
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






ut_test(test_trap_forwarding)
{
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
		.type = MD_VOLUME_TRAP,
		.id = "sam77",
		.status = 0
	    },
	    {
		.type = MD_NODE_TRAP,
		.id = "sam78",
		.status = 2
	    }
	};
    UT_ASSERT(md_messaging_setup());

    incoming_traps = traps;
    incoming_traps_count = 4;

    UT_ASSERT(md_trap_sender_queue_empty());
    md_event_list_loop();
    UT_ASSERT(!md_trap_sender_queue_empty());

    md_trap_sender_loop();
    UT_ASSERT(md_trap_sender_queue_empty());

    UT_ASSERT_EQUAL(1, sent_traps_count);
    UT_ASSERT_EQUAL(MD_NODE_TRAP, sent_traps[0].type);
    UT_ASSERT_EQUAL_STR("sam75", sent_traps[0].id);
    UT_ASSERT_EQUAL(2, sent_traps[0].status);

    md_event_list_loop();
    md_event_list_loop();
    md_event_list_loop();

    UT_ASSERT(!md_trap_sender_queue_empty());
    md_trap_sender_loop();

    /* all enqueued traps are sent in one loop */
    UT_ASSERT_EQUAL(4, sent_traps_count);
    UT_ASSERT_EQUAL(MD_DISK_TRAP, sent_traps[1].type);
    UT_ASSERT_EQUAL_STR("sam76", sent_traps[1].id);
    UT_ASSERT_EQUAL(1, sent_traps[1].status);
    UT_ASSERT_EQUAL(MD_VOLUME_TRAP, sent_traps[2].type);
    UT_ASSERT_EQUAL_STR("sam77", sent_traps[2].id);
    UT_ASSERT_EQUAL(0, sent_traps[2].status);
    UT_ASSERT_EQUAL(MD_NODE_TRAP, sent_traps[3].type);
    UT_ASSERT_EQUAL_STR("sam78", sent_traps[3].id);
    UT_ASSERT_EQUAL(2, sent_traps[3].status);

    UT_ASSERT(md_trap_sender_queue_empty());

    /* some loops without incoming trap ... */
    md_event_list_loop();
    md_event_list_loop();
    md_event_list_loop();
    UT_ASSERT(md_trap_sender_queue_empty());

    md_messaging_cleanup();

}


