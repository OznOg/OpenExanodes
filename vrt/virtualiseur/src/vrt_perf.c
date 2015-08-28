/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/clientd/include/nbd_clientd_perf.h" /* for nbd_clientd_get_exaperf */

#include "vrt/virtualiseur/include/vrt_perf.h"

#include "log/include/log.h"

#include "os/include/os_time.h"
#include "os/include/os_stdio.h"
#include "os/include/os_thread.h"

#define NB_REPART 8
#define NB_REPART_INTER 9
#define NB_REPART_DIST 18

#define __READ  0
#define __WRITE 1

static exaperf_sensor_t *vrt_req_size_repart[2];
static exaperf_sensor_t *vrt_req_dist[2];
static exaperf_sensor_t *vrt_req_inter[2];
static exaperf_sensor_t *vrt_iodepth[2];
static exaperf_sensor_t *vrt_req_dur[2];
static exaperf_sensor_t *vrt_req_nb_ios;
static exaperf_sensor_t *vrt_io_op_dur_first;
static exaperf_sensor_t *vrt_io_op_dur_second;
static exaperf_sensor_t *vrt_io_op_submit_diff;
static exaperf_sensor_t *vrt_io_op_end_diff;

static exaperf_sensor_t *rainX_sync_duration;
static exaperf_sensor_t *rainX_post_resync_duration;
static exaperf_sensor_t *rainX_stop_duration;

#define NB_DEBUG_SENSORS 5
static exaperf_sensor_t *debug_sensor[NB_DEBUG_SENSORS];

static double limits_vrt_req_size[NB_REPART] = {1, 16, 32, 64, 128, 256, 512};
static double limits_vrt_dist[NB_REPART_DIST] = {-1024, -512, -256, -128, -64, -32, -4, -1, 0, 1, 4, 32, 64, 128 , 256, 512, 1024};
static double limits_vrt_inter[NB_REPART_INTER] = {0, 1, 2, 3, 4, 5, 10, 15};

static uint64_t next_sector[2] = {0, 0};
static uint64_t last_req_time[2] = {0, 0};

static double iodepth = 0;
static os_thread_mutex_t iodepth_mutex;

static uint64_t get_date_diff(uint64_t date1, uint64_t date2)
{
    if (date2 > date1)
	return date2 - date1;
    else
	return date1 - date2;
}

static exaperf_t *eh_client = NULL;

void vrt_perf_init(void)
{
    unsigned int i;

    eh_client = nbd_clientd_get_exaperf();

    vrt_iodepth[__READ] = exaperf_counter_init(eh_client,
					     "VRT_IODEPTH___READ");
    vrt_iodepth[__WRITE] = exaperf_counter_init(eh_client,
					     "VRT_IODEPTH___WRITE");

    vrt_req_size_repart[__READ] = exaperf_repart_init(eh_client,
						    "VRT_REQ_SIZE_REPART___READ",
						    NB_REPART,
						    limits_vrt_req_size);
    vrt_req_size_repart[__WRITE] = exaperf_repart_init(eh_client,
						     "VRT_REQ_SIZE_REPART___WRITE",
						     NB_REPART,
						     limits_vrt_req_size);

    vrt_req_dist[__READ] = exaperf_repart_init(eh_client,
					     "VRT_REQ_DIST___READ",
					     NB_REPART_DIST,
					     limits_vrt_dist);

    vrt_req_dist[__WRITE] = exaperf_repart_init(eh_client,
					      "VRT_REQ_DIST___WRITE",
					      NB_REPART_DIST,
					      limits_vrt_dist);

    vrt_req_inter[__READ] = exaperf_repart_init(eh_client,
					      "VRT_REQ_INTER___READ",
					      NB_REPART_INTER,
					      limits_vrt_inter);
    vrt_req_inter[__WRITE] = exaperf_repart_init(eh_client,
					      "VRT_REQ_INTER___WRITE",
					      NB_REPART_INTER,
					      limits_vrt_inter);
    vrt_req_dur[__READ] = exaperf_duration_init(eh_client,
						"VRT_REQ_DUR___READ",
						true);
    vrt_req_dur[__WRITE] = exaperf_duration_init(eh_client,
						 "VRT_REQ_DUR___WRITE",
						 true);

    vrt_req_nb_ios = exaperf_counter_init(eh_client, "VRT_REQ_NB_IOS");

    vrt_io_op_dur_first = exaperf_duration_init(eh_client,
						"VRT_IO_OP_DUR_FIRST",
						true);

    vrt_io_op_dur_second = exaperf_duration_init(eh_client,
						 "VRT_IO_OP_DUR_SECOND",
						 true);

    vrt_io_op_submit_diff = exaperf_duration_init(eh_client,
						  "VRT_IO_OP_SUBMIT_DIFF",
						  true);

    vrt_io_op_end_diff = exaperf_duration_init(eh_client,
					       "VRT_IO_OP_END_DIFF",
					       true);

    rainX_sync_duration = exaperf_duration_init(eh_client,
                                                "RAINX_SYNC_DURATION",
                                                false);

    rainX_post_resync_duration = exaperf_duration_init(eh_client,
                                                "RAINX_POST_RESYNC_DURATION",
                                                false);

    rainX_stop_duration = exaperf_duration_init(eh_client,
                                                "RAINX_STOP_DURATION",
                                                false);

    for (i = 0; i < NB_DEBUG_SENSORS; i++)
    {
        char sensor_name[32];
        os_snprintf(sensor_name, 32, "VRT_DEBUG_%d", i);
        debug_sensor[i] = exaperf_duration_init(eh_client, sensor_name , false);
    }

    os_thread_mutex_init(&iodepth_mutex);
}

void vrt_perf_debug_begin(unsigned int num_debug)
{
    exaperf_duration_begin(debug_sensor[num_debug]);
}

void vrt_perf_debug_end(unsigned int num_debug)
{
    exaperf_duration_end(debug_sensor[num_debug]);
}

void vrt_perf_debug_flush(void)
{
    unsigned int i;

    for (i = 0; i < NB_DEBUG_SENSORS; i++)
        exaperf_sensor_flush(debug_sensor[i]);
}

void vrt_perf_make_request(vrt_io_type_t io_type, blockdevice_io_t *bio)
{
    uint64_t now_ms = os_gettimeofday_msec();
    size_t nbsect = BYTES_TO_SECTORS(bio->size);
    double dist;
    double inter_arrival;

    int rw = io_type == VRT_IO_TYPE_READ ? __READ : __WRITE;

    dist = (double)bio->start_sector - next_sector[rw];
    inter_arrival = (double)now_ms - last_req_time[rw];

    exaperf_repart_add_value(vrt_req_size_repart[rw], bio->size / 1024.);

    os_thread_mutex_lock(&iodepth_mutex);
    exaperf_counter_set(vrt_iodepth[rw], iodepth);
    iodepth++;
    os_thread_mutex_unlock(&iodepth_mutex);

    next_sector[rw] = bio->start_sector + nbsect;
    exaperf_repart_add_value(vrt_req_dist[rw], dist);

    last_req_time[rw] = now_ms;
    exaperf_repart_add_value(vrt_req_inter[rw], inter_arrival);

    bio->submit_date = now_ms;
}

void vrt_perf_end_request(struct vrt_request *vrt_req)
{
    double now = os_gettimeofday_msec();
    blockdevice_io_t *bio = vrt_req->ref_bio;

    int rw = vrt_req->iotype == VRT_IO_TYPE_READ ? __READ : __WRITE;

    os_thread_mutex_lock(&iodepth_mutex);
    iodepth--;
    os_thread_mutex_unlock(&iodepth_mutex);

    exaperf_duration_record(vrt_req_dur[rw], (double)now - bio->submit_date);
    exaperf_counter_set(vrt_req_nb_ios, vrt_req->nb_io_ops);


    /* record the time used by the first and second IOs (data and replica) for
     * one request
     *
     * Rq 1: this is relevant only for writes on RAINX
     * Rq 2: don't really know what the 'size' test is for I reproduce the
     *       behavior from 'proto_perfs' branch where this code was originally
     *       developped
     */
    if (vrt_req->io_list != NULL &&
        vrt_req->io_list->next != NULL &&
        vrt_req->io_list->size == vrt_req->io_list->next->size)
    {
        struct vrt_io_op *first_io = vrt_req->io_list;
        struct vrt_io_op *second_io = vrt_req->io_list->next;

        exaperf_duration_record(vrt_io_op_dur_first,
                                get_date_diff(first_io->end_date,
                                              first_io->submit_date));
        exaperf_duration_record(vrt_io_op_dur_second,
                                get_date_diff(second_io->end_date,
                                              second_io->submit_date));
        exaperf_duration_record(vrt_io_op_submit_diff,
                                get_date_diff(first_io->submit_date,
                                              second_io->submit_date));
        exaperf_duration_record(vrt_io_op_end_diff,
                                get_date_diff(first_io->end_date,
                                              second_io->end_date));
    }
}

void vrt_perf_io_op_submit(struct vrt_request *vrt_req, struct vrt_io_op *io_op)
{
    uint64_t now_ms = os_gettimeofday_msec();

    vrt_req->nb_io_ops++;
    io_op->submit_date = now_ms;
}

void vrt_perf_io_op_end(struct vrt_io_op *io_op)
{
    uint64_t now_ms = os_gettimeofday_msec();

    io_op->end_date = now_ms;
}

void rainx_perf_resync_slot_begin(void)
{
    exaperf_duration_begin(rainX_sync_duration);
}

void rainx_perf_resync_slot_end(void)
{
    exaperf_duration_end(rainX_sync_duration);
}

void rainx_perf_resync_slot_flush(void)
{
    exaperf_sensor_flush(rainX_sync_duration);
}

void rainx_perf_post_resync_begin(void)
{
    exaperf_duration_begin(rainX_post_resync_duration);
}

void rainx_perf_post_resync_end(void)
{
    exaperf_duration_end(rainX_post_resync_duration);
}

void rainx_perf_post_resync_flush(void)
{
    exaperf_sensor_flush(rainX_post_resync_duration);
}

void rainx_perf_stop_begin(void)
{
    exaperf_duration_begin(rainX_stop_duration);
}

void rainx_perf_stop_end(void)
{
    exaperf_duration_end(rainX_stop_duration);
}

void rainx_perf_stop_flush(void)
{
    exaperf_sensor_flush(rainX_stop_duration);
}
