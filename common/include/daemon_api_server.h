/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __DAEMON_API_SERVER_H
#define __DAEMON_API_SERVER_H

#include "examsg/include/examsg.h"

int admwrk_daemon_reply(ExamsgHandle mh, ExamsgID id,
                    const void *answer,  size_t answer_size);

int admwrk_daemon_ackinterrupt(ExamsgHandle mh, ExamsgID id);

#endif /* __DAEMON_API_SERVER_H */
