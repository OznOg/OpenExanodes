/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EVMGR_PUB_EVENTS_H__
#define __EVMGR_PUB_EVENTS_H__

#include "examsg/include/examsg.h"

/** Instance status */
typedef enum instance_state {
  INSTANCE_UP,
  INSTANCE_DOWN,
  INSTANCE_CHECK_UP,
  INSTANCE_CHECK_DOWN,
} instance_state_t;

/* WARNING keep aligned because this structure is exchanged between nodes */
typedef struct {
  ExamsgID id;             /**< service id */
  instance_state_t state;  /**< state */
  exa_nodeid_t node_id;    /**< node id */
} instance_event_t;

/** Event manager instance event */
EXAMSG_DCLMSG(instance_event_msg_t,
    instance_event_t event;
);


#endif
