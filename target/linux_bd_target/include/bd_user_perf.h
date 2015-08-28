/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __BD_USER_PERF_H__
#define __BD_USER_PERF_H__

#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"
#include "target/linux_bd_target/include/bd_user.h"

#ifdef WITH_PERF
#define BDEV_TARGET_PERF_INIT()	                        bdev_target_perf_init()
#define BDEV_TARGET_PERF_CLEANUP()                      bdev_target_perf_cleanup()
#define BDEV_TARGET_PERF_MAKE_REQUEST(rw, cmd, len)     bdev_target_perf_make_request(rw, cmd, len)
#define BDEV_TARGET_PERF_END_REQUEST(rw, cmd)           bdev_target_perf_end_request(rw, cmd)
#else
#define BDEV_TARGET_PERF_INIT()
#define BDEV_TARGET_PERF_CLEANUP()
#define BDEV_TARGET_PERF_MAKE_REQUEST(rw, cmd, len)
#define BDEV_TARGET_PERF_END_REQUEST(rw, cmd)
#endif

void bdev_target_perf_init(void);
void bdev_target_perf_cleanup(void);
void bdev_target_perf_make_request(int rw, struct bd_user_queue *op, double len);
void bdev_target_perf_end_request(int rw, struct bd_user_queue *op);

#endif
