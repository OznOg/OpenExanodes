/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "target/linux_bd_target/include/bd_user_perf.h"

#include "nbd/clientd/include/nbd_clientd_perf.h" /* for 'nbd_clientd_get_exaperf' */

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"
#include "os/include/os_thread.h"

#include <stdlib.h>
#include <stdarg.h>

#define TARGET_REPART 8
static double limits_target_req_size[TARGET_REPART] = {1, 16, 32, 64, 128, 256, 512};

#define TARGET_REPART_DEPTH 10
static double limits_target_iodepth[TARGET_REPART_DEPTH] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

static exaperf_sensor_t *target_req_time[2];
static exaperf_sensor_t *target_req_size_repart[2];
static exaperf_sensor_t *target_iodepth[2];

static double iodepth;
static os_thread_mutex_t iodepth_mutex;

static /* FIXME should be const */ exaperf_t *target_eh = NULL;

void bdev_target_perf_init(void)
{
    target_eh = nbd_clientd_get_exaperf();

    if (target_eh == NULL)
        return;

    target_iodepth[0] = exaperf_repart_init(target_eh,
                                            "BDEV_TARGET_IODEPTH_READ",
                                            TARGET_REPART_DEPTH,
                                            limits_target_iodepth);
    target_iodepth[1] = exaperf_repart_init(target_eh,
                                            "BDEV_TARGET_IODEPTH_WRITE",
                                            TARGET_REPART_DEPTH,
                                            limits_target_iodepth);

    target_req_time[0] = exaperf_duration_init(target_eh,
                                               "BDEV_TARGET_DUR_READ",
                                               true);
    target_req_time[1] = exaperf_duration_init(target_eh,
                                               "BDEV_TARGET_DUR_WRITE",
                                               true);

    target_req_size_repart[0] = exaperf_repart_init(target_eh,
                                                    "BDEV_TARGET_REQ_SIZE_READ",
                                                    TARGET_REPART,
                                                    limits_target_req_size);
    target_req_size_repart[1] = exaperf_repart_init(target_eh,
                                                    "BDEV_TARGET_REQ_SIZE_WRITE",
                                                    TARGET_REPART,
                                                    limits_target_req_size);

    os_thread_mutex_init(&iodepth_mutex);
}

void bdev_target_perf_cleanup(void)
{
    target_eh = NULL;
    os_thread_mutex_destroy(&iodepth_mutex);
}

void bdev_target_perf_make_request(int rw, struct bd_user_queue *op, double len)
{
    op->submit_date = os_gettimeofday_msec();
    exaperf_repart_add_value(target_req_size_repart[rw], len);

    os_thread_mutex_lock(&iodepth_mutex);
    exaperf_repart_add_value(target_iodepth[rw], iodepth);
    iodepth++;
    os_thread_mutex_unlock(&iodepth_mutex);
}

void bdev_target_perf_end_request(int rw, struct bd_user_queue *op)
{
    exaperf_duration_record(target_req_time[rw],
			    (double)(os_gettimeofday_msec() - op->submit_date));

    os_thread_mutex_lock(&iodepth_mutex);
    iodepth--;
    os_thread_mutex_unlock(&iodepth_mutex);
}


