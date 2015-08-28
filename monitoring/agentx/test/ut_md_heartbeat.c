/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/agentx/src/md_heartbeat.h"
#include "monitoring/md_com/include/md_com.h"
#include "monitoring/common/include/md_types.h"
#include "monitoring/common/include/md_constants.h"

#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <unit_testing.h>
#include <stdbool.h>

bool suicided = false;
bool answer_alive = true;


ut_setup()
{
    suicided = false;
    answer_alive = true;
}


ut_cleanup()
{

}


/* fake */
void md_send_msg(const md_com_msg_t* tx_msg)
{
    UT_ASSERT_EQUAL(MD_MSG_AGENT_ALIVE, tx_msg->type);
    if (answer_alive)
    {
	md_received_alive();
    }
}


/* fake */
void commit_suicide(void)
{
    suicided = true;
}

ut_test(test_heartbeat) __ut_lengthy
{
    UT_ASSERT(!md_is_alive());
    md_heartbeat_loop();
    UT_ASSERT(md_is_alive());
    UT_ASSERT(!suicided);
    md_heartbeat_loop();
    UT_ASSERT(md_is_alive());
    UT_ASSERT(!suicided);
    answer_alive = false;
    md_heartbeat_loop();
    UT_ASSERT(!suicided);
    os_sleep(MD_HEARTBEAT_TIMEOUT_SECONDS);
    md_heartbeat_loop();
    UT_ASSERT(suicided);
}


