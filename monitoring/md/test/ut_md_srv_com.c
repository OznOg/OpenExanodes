/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md/src/md_srv_com.h"
#include "monitoring/md_com/include/md_com_msg.h"
#include "monitoring/common/include/md_types.h"
#include "monitoring/common/include/md_constants.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <unit_testing.h>
#include <stdbool.h>


extern void md_srv_com_loop();



ut_setup()
{
}


ut_cleanup()
{

}


/* FIXME Added timeout because the thread join fails */
ut_test(test_alive_answering) __ut_timeout(60s)
{
    int ret;
    int connection;
    os_thread_t srv_com_thread;
    md_msg_agent_alive_t alive;
    md_com_msg_t *tx_msg;
    md_com_msg_t *rx_msg;

    UT_ASSERT(os_thread_create(&srv_com_thread, 0, md_srv_com_thread, NULL));
    os_sleep(1);

    /* connect to srv_com */
    ret = md_com_connect(MD_COM_SOCKET_PATH, &connection);
    UT_ASSERT(ret == COM_SUCCESS);

    /* send an alive message */
    tx_msg = md_com_msg_alloc_tx(MD_MSG_AGENT_ALIVE,
				 (const char*) &alive,
				 sizeof(md_msg_agent_alive_t));
    ret = md_com_send_msg(connection, tx_msg);
    UT_ASSERT(ret == COM_SUCCESS);
    md_com_msg_free_message(tx_msg);

    os_sleep(1);

    /* an alive message should have been answered */
    rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
    UT_ASSERT(rx_msg != NULL);

    ret = md_com_recv_msg(connection, rx_msg);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);
    UT_ASSERT_EQUAL(MD_MSG_AGENT_ALIVE, rx_msg->type);

    md_com_msg_free_message(rx_msg);

    md_srv_com_thread_stop();
    /* FIXME The join blocks: the thread never terminates, must be fixed */
    os_thread_join(srv_com_thread);
}
