/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __INSTANCE_H__
#define __INSTANCE_H__

#include "admind/src/adm_service.h"

/* Data structures */
typedef enum inst_op
{
  INST_OP_NOTHING,
  INST_OP_CHECK_DOWN,
  INST_OP_CHECK_UP,
  INST_OP_DOWN,
  INST_OP_UP
}
inst_op_t;

void inst_static_init(void);

void inst_set_all_instances_down(void); /* at clstop */

int inst_node_add(struct adm_node *node);
void inst_node_del(struct adm_node *node);

/* events from evmgr */
void inst_evt_up(const struct adm_node *node);
void inst_evt_down(const struct adm_node *node);
void inst_evt_check_down(const struct adm_node *node,
			 const struct adm_service *service);
void inst_evt_check_up(const struct adm_node *node,
		       const struct adm_service *service);

void inst_set_resources_changed_up(const struct adm_service *service);
void inst_set_resources_changed_down(const struct adm_service *service);
/* List of nodes that we run recovery up|down of */
void inst_get_nodes_going_up(const struct adm_service *service,
                             exa_nodeset_t* nodes);
void inst_get_nodes_going_down(const struct adm_service *service,
                               exa_nodeset_t* nodes);
/* List of nodes that run the current local command */
void inst_get_nodes_up(const struct adm_service *service,
                       exa_nodeset_t* nodes);
void inst_get_nodes_down(const struct adm_service *service,
                         exa_nodeset_t* nodes);

/* functions for rpc.c in run_command */
void inst_get_current_membership_cmd(const struct adm_service *service,
                                 exa_nodeset_t *membership);
void inst_get_current_membership_rec(const struct adm_service *service,
                                 exa_nodeset_t *membership);

bool inst_is_node_down_rec(exa_nodeid_t nid);
bool inst_is_node_down_cmd(exa_nodeid_t nid);

/* Check if a node has flags 'committed_up' and 'csupd_up' set to DOWN
 * for all services. */
int inst_is_node_stopped(const struct adm_node *node);

/* functions for daemon_api_client.c in daemon_query */
int inst_check_blockable_event(void);

inst_op_t inst_compute_recovery(void);

void adm_hierarchy_run_stop(admwrk_ctx_t *ctx, const stop_data_t *nodes_to_stop, cl_error_desc_t *err_desc);
#endif

