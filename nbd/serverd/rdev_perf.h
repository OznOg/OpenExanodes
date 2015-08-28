/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef RDEV_PERF
#define RDEV_PERF

#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"
#include "nbd/serverd/nbd_serverd.h"

#if WITH_PERF

void __rdev_perf_init(device_t *disk_device);
void __rdev_perf_make_request(device_t *disk_device, header_t *req_header);
void __rdev_perf_end_request(device_t *disk_device, header_t *req_header);

#define rdev_perf_init(disk)	                  __rdev_perf_init(disk)
#define rdev_perf_make_request(disk, req_header)  __rdev_perf_make_request(disk, req_header)
#define rdev_perf_end_request(disk, req_header)   __rdev_perf_end_request(disk, req_header)
#else
#define rdev_perf_init(disk)
#define rdev_perf_make_request(disk, req_header)
#define rdev_perf_end_request(disk, req_header)
#endif
#endif
