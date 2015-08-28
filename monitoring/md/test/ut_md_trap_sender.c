/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md/src/md_trap_sender.h"
#include "monitoring/md_com/include/md_com_msg.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <unit_testing.h>
#include <stdbool.h>


extern bool md_trap_sender_queue_empty();
extern void md_trap_sender_loop();


md_msg_agent_trap_t sent_traps[4096];
int sent_traps_count;



/* TODO would be nice to have a setup "per test case" added to unit testing framework */
static void setup()
{
    memset(&sent_traps, 0, sizeof(sent_traps));
    sent_traps_count = 0;
}




/* fake implementation for ut purpose */
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



ut_test(test_simple_enqueueing_dequeueing)
{
    md_trap_sender_error_code_t ret;
    md_msg_agent_trap_t etrap1 = {
	.type = MD_NODE_TRAP,
	.id = "sam75",
	.status = 2
    };

    md_msg_agent_trap_t etrap2 = {
	.type = MD_DISK_TRAP,
	.id = "sam76",
	.status = 1
    };

    setup();

    UT_ASSERT(md_trap_sender_queue_empty());

    ret = md_trap_sender_enqueue(&etrap1);
    UT_ASSERT_EQUAL(MD_TRAP_SENDER_SUCCESS, ret);
    UT_ASSERT(!md_trap_sender_queue_empty());

    ret = md_trap_sender_enqueue(&etrap2);
    UT_ASSERT_EQUAL(MD_TRAP_SENDER_SUCCESS, ret);
    UT_ASSERT(!md_trap_sender_queue_empty());

    /* all enqueued traps are sent in one loop */
    md_trap_sender_loop();
    UT_ASSERT_EQUAL(2, sent_traps_count);
    UT_ASSERT_EQUAL(etrap1.type, sent_traps[0].type);
    UT_ASSERT_EQUAL_STR(etrap1.id, sent_traps[0].id);
    UT_ASSERT_EQUAL(etrap2.type, sent_traps[1].type);
    UT_ASSERT_EQUAL_STR(etrap2.id, sent_traps[1].id);
    UT_ASSERT_EQUAL(etrap2.status, sent_traps[1].status);

    UT_ASSERT(md_trap_sender_queue_empty());

    md_trap_sender_loop();
    md_trap_sender_loop();

    UT_ASSERT_EQUAL(2, sent_traps_count);

}


ut_test(test_queue_overflow)
{
    int i;
    md_trap_sender_error_code_t ret;
    md_msg_agent_trap_t etrap1 = {
	.type = MD_NODE_TRAP,
	.id = "sam75",
	.status = 2
    };
    setup();

    for (i=0; i<MD_TRAP_SENDER_QUEUE_SIZE; ++i)
    {
	ret = md_trap_sender_enqueue(&etrap1);
	UT_ASSERT_EQUAL(MD_TRAP_SENDER_SUCCESS, ret);
    }
    ret = md_trap_sender_enqueue(&etrap1);
    UT_ASSERT_EQUAL(MD_TRAP_SENDER_OVERFLOW, ret);

    md_trap_sender_loop();
    UT_ASSERT(md_trap_sender_queue_empty());
}




void test_enqueuing_thread(void* arg)
{
    int i;
    md_trap_sender_error_code_t ret;
    md_msg_agent_trap_t *payload = (md_msg_agent_trap_t *)arg;

    /* take care not exceeding QUEUE_SIZE ! */
    for (i=0; i<100; ++i)
    {
	ret = md_trap_sender_enqueue(payload);
	UT_ASSERT_EQUAL(MD_TRAP_SENDER_SUCCESS, ret);
	/* sleep a bit, to force msg interleaving */
	os_millisleep(1);
    }
}




ut_test(test_parallel_enqueueing_simple_dequeueing)
{
    int i;
    os_thread_t enqueuing_threads[3];
    md_msg_agent_trap_t etrap1 = {
	.type = MD_NODE_TRAP,
	.id = "sam75",
	.status = 2
    };

    md_msg_agent_trap_t etrap2 = {
	.type = MD_DISK_TRAP,
	.id = "sam76",
	.status = 1
    };

    md_msg_agent_trap_t etrap3 = {
	.type = MD_VOLUME_TRAP,
	.id = "sam77",
	.status = 3
    };

    setup();

    os_thread_create(&enqueuing_threads[0], 0,
		   test_enqueuing_thread,
		   &etrap1);
    os_thread_create(&enqueuing_threads[1], 0,
		   test_enqueuing_thread,
		   &etrap2);
    os_thread_create(&enqueuing_threads[2], 0,
		   test_enqueuing_thread,
		   &etrap3);

    os_thread_join(enqueuing_threads[0]);
    os_thread_join(enqueuing_threads[1]);
    os_thread_join(enqueuing_threads[2]);


    UT_ASSERT(!md_trap_sender_queue_empty());
    i = 0;
    while (!md_trap_sender_queue_empty())
    {
	md_trap_sender_loop();
	if (sent_traps[i].type == etrap1.type)
	{
	    UT_ASSERT_EQUAL_STR(etrap1.id, sent_traps[i].id);
	    UT_ASSERT_EQUAL(etrap1.status, sent_traps[i].status);
	}
	else if (sent_traps[i].type == etrap2.type)
	{
	    UT_ASSERT_EQUAL_STR(etrap2.id, sent_traps[i].id);
	    UT_ASSERT_EQUAL(etrap2.status, sent_traps[i].status);
	}
	else if (sent_traps[i].type == etrap3.type)
	{
	    UT_ASSERT_EQUAL_STR(etrap3.id, sent_traps[i].id);
	    UT_ASSERT_EQUAL(etrap3.status, sent_traps[i].status);
	}
	else
	{
	    UT_FAIL();
	}
	++i;
    }

}

