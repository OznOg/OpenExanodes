/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/serverd/nbd_serverd_perf.h"
#include "nbd/serverd/rdev_perf.h"
#include "os/include/os_time.h"
#include "log/include/log.h"
#include <stdio.h>

#define NB_RDEV_REPART_INTER 9

#define __READ  0
#define __WRITE 1

static double limits_inter[NB_RDEV_REPART_INTER] = {0, 1, 2, 3, 4, 5, 10, 15};

void __rdev_perf_init(device_t *disk_device)
{
    exaperf_t *eh = serverd_get_exaperf();
    exa_uuid_str_t device_uuid_str;

    uuid2str(&disk_device->uuid, device_uuid_str);


    disk_device->rdev_dur[__READ] =
	exaperf_duration_init_from_template(eh, "RDEV_DUR_READ", device_uuid_str, true);

    disk_device->rdev_dur[__WRITE] =
	exaperf_duration_init_from_template(eh, "RDEV_DUR_WRITE", device_uuid_str, true);

    disk_device->inter_arrival_repart[__READ] =
	exaperf_repart_init_from_template(eh, "RDEV_INTERARRIVAL_READ", device_uuid_str,
					  NB_RDEV_REPART_INTER, limits_inter);

    disk_device->inter_arrival_repart[__WRITE] =
	exaperf_repart_init_from_template(eh, "RDEV_INTERARRIVAL_WRITE", device_uuid_str,
					  NB_RDEV_REPART_INTER, limits_inter);

    disk_device->last_req_time[__READ] = 0;
    disk_device->last_req_time[__WRITE] = 0;
}

void __rdev_perf_make_request(device_t *disk_device, header_t *req_header)
{
    uint64_t now_ms = os_gettimeofday_msec();
    uint64_t inter_arrival;
    int rw = -1;

    EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(req_header->io.request_type));
    switch (req_header->io.request_type)
    {
    case NBD_REQ_TYPE_READ:
        rw = __READ;
        break;
    case NBD_REQ_TYPE_WRITE:
        rw = __WRITE;
        break;
    }

    /* WARNING: be careful, although the first call is taken into account
     * here (for the inter-arrival time), the first exaperf log should not
     * be taken into consideration for the analysis. Max and mean value
     * are very big because of the history of the IOs. This happens even
     * if we start/stop the cluser between two experiments. */
    if (disk_device->last_req_time[rw] != 0)
    {
	inter_arrival = now_ms - disk_device->last_req_time[rw];
	exaperf_repart_add_value(disk_device->inter_arrival_repart[rw],
				 inter_arrival);
    }
    disk_device->last_req_time[rw] = now_ms;
    req_header->io.rdev_submit_date = now_ms;
}

void __rdev_perf_end_request(device_t *disk_device, header_t *req_header)
{
    double duration;
    int rw = -1;

    EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(req_header->io.request_type));
    switch (req_header->io.request_type)
    {
    case NBD_REQ_TYPE_READ:
        rw = __READ;
        break;
    case NBD_REQ_TYPE_WRITE:
        rw = __WRITE;
        break;
    }

    duration = (double)os_gettimeofday_msec() - req_header->io.rdev_submit_date;

    exaperf_duration_record(disk_device->rdev_dur[rw], duration);
}
