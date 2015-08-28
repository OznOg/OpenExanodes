/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _VRT_INIT
#define _VRT_INIT

void vrt_init(int adm_my_id, int max_requests, exa_bool_t io_barriers,
              int rebuilding_slowdown_ms,
              int degraded_rebuilding_slowdown_ms);

void vrt_exit(void);

#endif

