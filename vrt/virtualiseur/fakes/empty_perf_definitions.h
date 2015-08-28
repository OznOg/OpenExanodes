/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_PERF_FAKES_H__
#define __UT_VRT_PERF_FAKES_H__

#include "vrt/virtualiseur/include/vrt_perf.h"

void vrt_perf_debug_begin(unsigned int num_debug) { }
void vrt_perf_debug_end(unsigned int num_debug) { }
void vrt_perf_debug_flush(void) { }

void rainx_perf_resync_slot_begin(void) { }
void rainx_perf_resync_slot_end(void) { }
void rainx_perf_resync_slot_flush(void) { }

void rainx_perf_post_resync_begin(void) { }
void rainx_perf_post_resync_end(void) { }
void rainx_perf_post_resync_flush(void) { }

void rainx_perf_stop_begin(void) { }
void rainx_perf_stop_end(void) { }
void rainx_perf_stop_flush(void) { }

#endif
