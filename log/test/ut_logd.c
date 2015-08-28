/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "log/include/log.h"
#include "log/src/logd_com.h"
#include "log/src/logd.h"
#include <string.h>
#include "os/include/os_file.h"

#define TEST_LOGFILE_NAME "dummy_log.delme"

static int count = 0;
#define NB_ITER 150
static bool com_inited = false;

int logd_com_init(void)
{
    com_inited = true;
    return 0;
}
void logd_com_exit(void)
{
    com_inited = false;
}

/*
 * Make a fake recv function that returns dummy logs in order to test
 * the main loop itself */
int logd_com_recv(exalog_data_t *out)
{

    exalog_msg_t *msg = &out->d.log_msg;
    /* When NB_ITER is reached, we send a QUIT message in order to
     * exit the loop (and thus, the test) */
    if (count >= NB_ITER)
    {
	out->type = LOG_QUIT;
	return 0;
    }

    out->type = LOG_MSG;

    memset(msg, 0, sizeof(*msg));

    strcpy(msg->file, "The_file");
    strcpy(msg->func, "The_function");
    msg->line = count++;

    /* make sure the NONE level is not sent */
    msg->level = (count % (EXALOG_LEVEL_LAST - 1 - EXALOG_LEVEL_FIRST)) + 1 + EXALOG_LEVEL_FIRST;

    msg->cid = EXAMSG_TEST_ID;

    if (count % 2)
	msg->lost = count;
    else
	msg->lost = 0;

    strcpy(msg->msg, "This_is_a_test");

    return 0;
}

ut_setup()
{
    unlink(TEST_LOGFILE_NAME);
}

ut_test(log_loop_test)
{
    int err;
    err = log_init(TEST_LOGFILE_NAME);
    UT_ASSERT(err == 0);
    UT_ASSERT(com_inited);

    exalog_loop(NULL);

    logd_com_exit();

    UT_ASSERT(count == NB_ITER);
    UT_ASSERT(!com_inited);
}

ut_cleanup()
{}

