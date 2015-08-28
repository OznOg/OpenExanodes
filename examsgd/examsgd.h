/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXAMSGD_H__
#define __EXAMSGD_H__

#include "common/include/exa_nodeset.h"

#include "examsg/include/examsg.h"

/** Default multicast address for networked messages */
#define EXAMSG_MCASTIP		"229.230.231.233"

/** Default communication port for networked messages */
#define EXAMSG_PORT		30798

/** Declare a node to examsgd at init */
EXAMSG_DCLMSG(examsg_node_info_msg_t, struct {
  exa_nodeid_t node_id;                    /**< Node id */
  char node_name[EXA_MAXSIZE_HOSTNAME+1];  /**< Node name */
});

/* asks examsgd to fence/unfence a set of node */
typedef struct {
  exa_nodeset_t node_set;
  enum fencing_order {
    FENCE, UNFENCE
  } order;
} examsgd_fencing_req_t;

/** Supervision node event */
EXAMSG_DCLMSG(examsgd_fencing_req_msg_t,
    examsgd_fencing_req_t request;
);


#endif
