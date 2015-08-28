/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SUP_VIEW_H__
#define __SUP_VIEW_H__

#include "csupd/include/exa_csupd.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"

/** A node state */
typedef enum sup_state
  {
    SUP_STATE_UNKNOWN,
    SUP_STATE_CHANGE,
    SUP_STATE_ACCEPT,
    SUP_STATE_COMMIT,
  } sup_state_t;

#define SUP_STATE_CHECK(state)  \
  ((state) >= SUP_STATE_UNKNOWN && (state) <= SUP_STATE_COMMIT)

const char *sup_state_name(sup_state_t state);

/** A node's view */
typedef struct sup_view
  {
    sup_state_t state;          /**< Node state */
    unsigned num_seen;          /**< Number of nodes seen */
    exa_nodeset_t nodes_seen;   /**< Nodes seen */
    exa_nodeset_t clique;       /**< Work clique */
    exa_nodeid_t coord;         /**< Coordinator */
    sup_gen_t accepted;         /**< Generation of last accepted membership */
    sup_gen_t committed;        /**< Generation of last committed membership */
    int pad1;
  } sup_view_t;

void sup_view_init(sup_view_t *view);

void sup_view_add_node(sup_view_t *view, exa_nodeid_t node_id);
void sup_view_del_node(sup_view_t *view, exa_nodeid_t node_id);

void sup_view_copy(sup_view_t *dest, const sup_view_t *src);

bool sup_view_equals(const sup_view_t *v1, const sup_view_t *v2);

void sup_view_debug(const sup_view_t *view);

#endif
