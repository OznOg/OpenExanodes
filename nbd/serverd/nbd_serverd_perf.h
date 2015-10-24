/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef SERVERD_PERF
#define SERVERD_PERF

#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"
#include "nbd/common/nbd_common.h"

#ifdef WITH_PERF
exaperf_t *serverd_get_exaperf(void);

int  __serverd_perf_init(void);
void __serverd_perf_cleanup(void);
void __serverd_perf_sensor_init(void);
void __serverd_perf_make_request(const nbd_io_desc_t *io);
void __serverd_perf_end_request(const nbd_io_desc_t *io);

#define serverd_perf_init()                    __serverd_perf_init()
#define serverd_perf_cleanup()                 __serverd_perf_cleanup()
#define serverd_perf_sensor_init()             __serverd_perf_sensor_init()
#define serverd_perf_make_request(io)  __serverd_perf_make_request(io)
#define serverd_perf_end_request(io)   __serverd_perf_end_request(io)
#else
#define serverd_perf_init()                    EXA_SUCCESS
#define serverd_perf_cleanup()
#define serverd_perf_sensor_init()
#define serverd_perf_make_request(io)
#define serverd_perf_end_request(io)
#endif

#endif
