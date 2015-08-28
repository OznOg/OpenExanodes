/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_H__
#define __RAIN1_H__

int rain1_init(int rebuilding_slowdown_ms,
               int degraded_rebuilding_slowdown_ms);

void rain1_cleanup(void);
#endif
