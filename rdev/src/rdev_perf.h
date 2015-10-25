/*
 * Copyright 2002, 2009 Seanodes SA http://www.seanodes.com. All rights
 * reserved and protected by French, U.S. and other countries' copyright laws.
 */

#ifndef RDEV_PERF
#define RDEV_PERF

#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"

typedef struct {
    uint64_t rdev_submit_date; /**< Data of the reqest submition to rdev in serverd */
    bool read;
} rdev_req_perf_t;

#ifdef WITH_PERF
typedef struct {
    exaperf_sensor_t *rdev_dur[2];
    exaperf_sensor_t *inter_arrival_repart[2];
    uint64_t last_req_time[2];
} rdev_perfs_t;
#endif

#if WITH_PERF

void __rdev_perf_init(rdev_perfs_t *rdev_perfs, const char *path);
void __rdev_perf_make_request(rdev_perfs_t *rdev_perfs, bool read, rdev_req_perf_t *req_perfs);
void __rdev_perf_end_request(rdev_perfs_t *rdev_perfs, const rdev_req_perf_t *req_perfs);

#define rdev_perf_init(disk, path)	                     __rdev_perf_init(disk, path)
#define rdev_perf_make_request(rdev_perfs, read, req_perfs)  __rdev_perf_make_request(rdev_perfs, read, req_perfs)
#define rdev_perf_end_request(rdev_perfs, req_perfs)         __rdev_perf_end_request(rdev_perfs, req_perfs)
#else
#define rdev_perf_init(disk, path)
#define rdev_perf_make_request(rdev_perfs, read, req_perfs)
#define rdev_perf_end_request(rdev_perfs, req_perfs)
#endif
#endif
