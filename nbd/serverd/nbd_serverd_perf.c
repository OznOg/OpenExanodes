/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "exaperf/include/exaperf.h"
#include "nbd/common/nbd_common.h"
#include "nbd/serverd/nbd_serverd_perf.h"

#include "common/include/exa_perf_instance.h" /* for 'exa_perf_instance_get' */

#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"

#include <stdlib.h>
#include <stdarg.h>

#define NB_REPART 8
#define NB_REPART_INTER 9
#define NB_REPART_DIST 18

static double limits_nbd_server_req[NB_REPART] = {1, 16, 32, 64, 128, 256, 512};
static double limits_inter[NB_REPART_INTER] = {0, 1, 2, 3, 4, 5, 10, 15};
static double limits_dist[NB_REPART_DIST] = {-1024, -512, -256, -128, -64, -32, -4, -1, 0, 1, 4, 32, 64, 128 , 256, 512, 1024};

/* Limits lba are given in MB */
#define NB_REPART_LBA 9

#define __READ  0
#define __WRITE 1

static double limits_lba[NB_REPART_LBA] = {2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384};
static exaperf_sensor_t *lba_repart[2];

static exaperf_sensor_t *req_size_repart[2];
static exaperf_sensor_t *inter_arrival_repart[2];
static exaperf_sensor_t *distance_repart[2];
static exaperf_sensor_t *header_dur[2];
static exaperf_sensor_t *data_dur;

static uint64_t last_req_time[2] = {0, 0};
static uint64_t next_sector[2] = {0, 0};

void __serverd_perf_sensor_init(void)
{
    exaperf_t *eh = exa_perf_instance_get();

    EXA_ASSERT_VERBOSE(eh != NULL, "Exaperf handle is nil");

    req_size_repart[__READ] = exaperf_repart_init(eh, "NBD_SERVER_REQ_SIZE_READ",
                                                NB_REPART, limits_nbd_server_req);
    req_size_repart[__WRITE] = exaperf_repart_init(eh, "NBD_SERVER_REQ_SIZE_WRITE",
						 NB_REPART, limits_nbd_server_req);

    inter_arrival_repart[__READ] = exaperf_repart_init(eh, "NBD_SERVER_INTERARRIVAL_READ",
						     NB_REPART_INTER, limits_inter);
    inter_arrival_repart[__WRITE] = exaperf_repart_init(eh, "NBD_SERVER_INTERARRIVAL_WRITE",
                                                      NB_REPART_INTER, limits_inter);

    lba_repart[__READ] = exaperf_repart_init(eh, "NBD_SERVER_LBA_READ",
					   NB_REPART_LBA, limits_lba);
    lba_repart[__WRITE] = exaperf_repart_init(eh, "NBD_SERVER_LBA_WRITE",
					    NB_REPART_LBA, limits_lba);

    distance_repart[__READ] = exaperf_repart_init(eh, "NBD_SERVER_DISTANCE_READ",
                                                NB_REPART_DIST, limits_dist);
    distance_repart[__WRITE] = exaperf_repart_init(eh, "NBD_SERVER_DISTANCE_WRITE",
						 NB_REPART_DIST, limits_dist);

    header_dur[__READ] = exaperf_duration_init(eh,"NBD_SERVER_HEADER_DUR_READ", true);
    header_dur[__WRITE] = exaperf_duration_init(eh, "NBD_SERVER_HEADER_DUR_WRITE", true);

    data_dur = exaperf_duration_init(eh, "NBD_SERVER_DATA_DUR_WRITE", true);
}

void __serverd_perf_make_request(const nbd_io_desc_t *io)
{
    uint64_t lba_in_kbytes;
    uint64_t now_ms;
    double inter_arrival;
    double dist;
    int rw = -1;

    EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(io->request_type));
    switch (io->request_type)
    {
    case NBD_REQ_TYPE_READ:
        rw = __READ;
        break;
    case NBD_REQ_TYPE_WRITE:
        rw = __WRITE;
        break;
    }

    now_ms = os_gettimeofday_msec();

    lba_in_kbytes = io->sector / 2;
    /* add the lba in MB in the repartition */
    exaperf_repart_add_value(lba_repart[rw], lba_in_kbytes / 1024);

    ((nbd_io_desc_t *)io)->header_submit_date = now_ms;

    inter_arrival = (double)now_ms - last_req_time[rw];
    dist = (double)io->sector - next_sector[rw];

    exaperf_repart_add_value(inter_arrival_repart[rw], inter_arrival);
    exaperf_repart_add_value(distance_repart[rw], dist);
    exaperf_repart_add_value(req_size_repart[rw],
            (io->sector_nb/2.));
    next_sector[rw] = io->sector + io->sector_nb;
    last_req_time[rw] = now_ms;
}

void __serverd_perf_end_request(const nbd_io_desc_t *io)
{
    double now = os_gettimeofday_msec();
    int rw = -1;

    EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(io->request_type));
    switch (io->request_type)
    {
    case NBD_REQ_TYPE_READ:
        rw = __READ;
        break;
    case NBD_REQ_TYPE_WRITE:
        rw = __WRITE;
        break;
    }

    exaperf_duration_record(header_dur[rw],
			    (double)now - io->header_submit_date);

    if (rw == __WRITE)
	exaperf_duration_record(data_dur,
				(double)now - io->data_submit_date);
}


