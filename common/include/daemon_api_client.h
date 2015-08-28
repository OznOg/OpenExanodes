/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __DAEMON_API_CLIENT_H
#define __DAEMON_API_CLIENT_H

#include "examsg/include/examsg.h"

int  admwrk_daemon_query(ExamsgHandle mh, ExamsgID to, ExamsgType request_type,
                         const void *request, size_t request_size,
                         void *answer,  size_t answer_size);

int  admwrk_daemon_query_nointr(ExamsgHandle mh, ExamsgID to,
				ExamsgType request_type,
				const void *request, size_t request_size,
				void *answer, size_t answer_size);

#endif /* __DAEMON_API_H */
