/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SUP_PING_H__
#define __SUP_PING_H__

#include "sup_cluster.h"
#include "sup_view.h"

#include "csupd/include/exa_csupd.h"

extern int ping_period;   	/**< Ping period, in seconds */
extern bool do_ping;	/**< Whether a ping must be sent */

typedef struct sup_ping
  {
    exa_nodeid_t sender;         /**< Sender id */
    unsigned short incarnation;  /**< Sender's incarnation */
    sup_view_t view;             /**< Sender's view */
  } sup_ping_t;

int sup_deliver(sup_gen_t gen, const exa_nodeset_t *mship);

bool sup_check_ping(const sup_ping_t *ping, char op);

void sup_send_ping(const sup_cluster_t *cluster, const sup_view_t *view);
bool sup_recv_ping(sup_ping_t *ping);

bool sup_setup_messaging(exa_nodeid_t local_id);
void sup_cleanup_messaging(void);

#endif
