/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LUM_EXECUTIVE_H
#define __LUM_EXECUTIVE_H

#include "examsg/include/examsg.h"

extern ExamsgHandle lum_mh;

int lum_thread_create(void);
int lum_thread_stop(void);

#endif /* __LUM_EXECUTIVE_H */
