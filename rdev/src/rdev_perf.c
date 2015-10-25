/*
 * Copyright 2002, 2009 Seanodes SA http://www.seanodes.com. All rights
 * reserved and protected by French, U.S. and other countries' copyright laws.
 */

#include "rdev/src/rdev_perf.h"

#include "common/include/exa_perf_instance.h" /* for 'exa_perf_instance_get' */

#include "os/include/os_time.h"
#include "log/include/log.h"
#include <stdio.h>

#define NB_RDEV_REPART_INTER 9

#define __READ  0
#define __WRITE 1

static double limits_inter[NB_RDEV_REPART_INTER] = {0, 1, 2, 3, 4, 5, 10, 15};

void __rdev_perf_init(rdev_perfs_t *rdev_perfs, const char *path)
{
    exaperf_t *eh = exa_perf_instance_get();

    rdev_perfs->rdev_dur[__READ] =
	exaperf_duration_init_from_template(eh, "RDEV_DUR_READ", path, true);

    rdev_perfs->rdev_dur[__WRITE] =
	exaperf_duration_init_from_template(eh, "RDEV_DUR_WRITE", path, true);

    rdev_perfs->inter_arrival_repart[__READ] =
	exaperf_repart_init_from_template(eh, "RDEV_INTERARRIVAL_READ", path,
					  NB_RDEV_REPART_INTER, limits_inter);

    rdev_perfs->inter_arrival_repart[__WRITE] =
	exaperf_repart_init_from_template(eh, "RDEV_INTERARRIVAL_WRITE", path,
					  NB_RDEV_REPART_INTER, limits_inter);

    rdev_perfs->last_req_time[__READ] = 0;
    rdev_perfs->last_req_time[__WRITE] = 0;
}

void __rdev_perf_make_request(rdev_perfs_t *rdev_perfs, bool read, rdev_req_perf_t *req_perfs)
{
    uint64_t now_ms = os_gettimeofday_msec();
    uint64_t inter_arrival;
    int rw = read ? __READ : __WRITE;

    /* WARNING: be careful, although the first call is taken into account
     * here (for the inter-arrival time), the first exaperf log should not
     * be taken into consideration for the analysis. Max and mean value
     * are very big because of the history of the IOs. This happens even
     * if we start/stop the cluser between two experiments. */
    if (rdev_perfs->last_req_time[rw] != 0)
    {
	inter_arrival = now_ms - rdev_perfs->last_req_time[rw];
	exaperf_repart_add_value(rdev_perfs->inter_arrival_repart[rw],
				 inter_arrival);
    }
    rdev_perfs->last_req_time[rw] = now_ms;
    req_perfs->rdev_submit_date = now_ms;
    req_perfs->read = read;
}

void __rdev_perf_end_request(rdev_perfs_t *rdev_perfs, const rdev_req_perf_t *req_perfs)
{
    double duration = (double)os_gettimeofday_msec() - req_perfs->rdev_submit_date;
    int rw = req_perfs->read ? __READ : __WRITE;

    exaperf_duration_record(rdev_perfs->rdev_dur[rw], duration);
}
