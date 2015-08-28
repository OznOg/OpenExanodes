/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXAMSGD_CLIENT_H__
#define __EXAMSGD_CLIENT_H__

#include "common/include/exa_nodeset.h"

#include "examsg/include/examsg.h"

int examsgAddNode(ExamsgHandle mh, exa_nodeid_t node_id, const char *host);
int examsgDelNode(ExamsgHandle mh, exa_nodeid_t node_id);

int examsgFence(ExamsgHandle mh, const exa_nodeset_t *node_set);

int examsgUnfence(ExamsgHandle mh, const exa_nodeset_t *node_set);

int examsgdExit(ExamsgHandle mh);

#endif
