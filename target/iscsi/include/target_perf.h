/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TARGET_PERF
#define TARGET_PERF

#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"
#include "target/iscsi/include/target.h"

#ifdef WITH_PERF
#define ISCSI_TARGET_PERF_INIT()	                iscsi_target_perf_init()
#define ISCSI_TARGET_PERF_CLEANUP()	                iscsi_target_perf_cleanup()
#define ISCSI_TARGET_PERF_MAKE_READ_REQUEST(cmd, len)	iscsi_target_perf_make_request(0, cmd, len)
#define ISCSI_TARGET_PERF_MAKE_WRITE_REQUEST(cmd, len)	iscsi_target_perf_make_request(1, cmd, len)
#define ISCSI_TARGET_PERF_END_READ_REQUEST(cmd)	        iscsi_target_perf_end_request(0, cmd)
#define ISCSI_TARGET_PERF_END_WRITE_REQUEST(cmd)	iscsi_target_perf_end_request(1, cmd)
#else
#define ISCSI_TARGET_PERF_INIT()
#define ISCSI_TARGET_PERF_CLEANUP()
#define ISCSI_TARGET_PERF_MAKE_READ_REQUEST(cmd, len)
#define ISCSI_TARGET_PERF_MAKE_WRITE_REQUEST(cmd, len)
#define ISCSI_TARGET_PERF_END_READ_REQUEST(cmd)
#define ISCSI_TARGET_PERF_END_WRITE_REQUEST(cmd)
#endif

void iscsi_target_perf_init(void);
void iscsi_target_perf_cleanup(void);
void iscsi_target_perf_make_request(int rw, TARGET_CMD_T *cmd, double len);
void iscsi_target_perf_end_request(int rw, TARGET_CMD_T *cmd);

#endif
