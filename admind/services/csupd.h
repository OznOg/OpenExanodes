/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __CSUPD__H
#define __CSUPD__H

#include "examsg/include/examsg.h"

int csupd_init(void);
int csupd_shutdown(void);

int examsg_init(void);
int examsg_shutdown(ExamsgHandle mh);


#endif /* __CSUPD__H */
