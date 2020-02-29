/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_CLUSTER_H
#define __ADM_CLUSTER_H

#include "admind/src/service_parameter.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"
#include "os/include/os_inttypes.h"
#include "vrt/common/include/spof.h"

/* Data structures */

typedef enum {
  ADM_CLUSTER_GOAL_UNDEFINED = 0,
  ADM_CLUSTER_GOAL_STARTED,
  ADM_CLUSTER_GOAL_STOPPED,
} adm_cluster_goal_t;

struct adm_node;
struct adm_volume;
struct adm_disk;
struct adm_service;

struct adm_cluster_param
{
  char name[EXA_MAXSIZE_PARAM_NAME + 1];
  char default_value[EXA_MAXSIZE_PARAM_VALUE + 1];
  char value[EXA_MAXSIZE_PARAM_VALUE + 1];
  bool set;
};

/** Definition for the cluster-wide monitoring parameters
    This structure must be able to go through the network
    !!! TODO : alignement problems !!! */
struct adm_cluster_monitoring
{
  char snmpd_host[EXA_MAXSIZE_HOSTNAME + 1];    /**< Machine hostname of the machine
						     that hosts snmpd */
  uint32_t snmpd_port;            /**< port to use. '0' means DEFAULT */
  bool started;                   /**< Is monitoring started ? */
};


struct adm_cluster
{
  char name[EXA_MAXSIZE_CLUSTERNAME + 1];
  exa_uuid_t uuid;
  adm_cluster_goal_t goal;
  adm_cluster_goal_t persistent_goal;
  int64_t version;
  int created;
  struct adm_node *nodes[EXA_MAX_NODES_NUMBER];
  unsigned int nodes_number;
  unsigned int disks_number;
  struct adm_cluster_param params[NBMAX_PARAMS];
  struct adm_cluster_monitoring monitoring_parameters;
};


/* Extern variables */

extern struct adm_cluster adm_cluster;
extern char *adm_config_buffer;
extern int adm_config_size;
extern exa_nodeid_t adm_my_id;
extern exa_nodeid_t adm_leader_id;
extern bool   adm_leader_set;


/* Functions */

void adm_cluster_init(void);
void adm_cluster_cleanup(void);

void adm_cluster_lock(void);
void adm_cluster_unlock(void);

bool adm_is_leader(void);

int adm_cluster_insert_node(struct adm_node *node);
struct adm_node *adm_cluster_remove_node(const struct adm_node *node);
unsigned int adm_cluster_nb_nodes(void);
const struct adm_node *adm_cluster_get_node_by_id(exa_nodeid_t id);
const struct adm_node *adm_cluster_get_node_by_name(const char *name);
struct adm_node *adm_cluster_first_node_at(exa_nodeid_t id);

int adm_cluster_insert_disk(struct adm_disk *disk);
struct adm_disk *adm_cluster_remove_disk(const exa_uuid_t *disk_uuid);
unsigned int adm_cluster_nb_disks(void);
struct adm_disk *adm_cluster_get_disk_by_path(const char *node_name,
					      const char *disk_path);
struct adm_disk *adm_cluster_get_disk_by_uuid(const exa_uuid_t *uuid);

struct adm_volume *adm_cluster_get_volume_by_name(const char *group_name,
						  const char *volume_name);
struct adm_volume *adm_cluster_get_volume_by_uuid(const exa_uuid_t *uuid);

int adm_cluster_set_param(const char *name, const char *value);
int adm_cluster_set_param_default(const char *name, const char *default_value);
int adm_cluster_set_param_to_default(const char *name);
const char *adm_cluster_get_param_default(const char *name);
const char *adm_cluster_get_param_text(const char *name);
int adm_cluster_get_param_int(const char *name);
int adm_cluster_get_param_boolean(const char *name);
void adm_cluster_get_param_nodeset(exa_nodeset_t *nodeset, const char *name);
void adm_cluster_log_tuned_params(void);

const struct adm_node *adm_myself(void);
const struct adm_node *adm_leader(void);

int adm_cluster_save_goal(adm_cluster_goal_t goal);
int adm_cluster_load_goal(adm_cluster_goal_t *goal);
int adm_cluster_delete_goal(void);

/* Macros */

#define adm_cluster_for_each_node(node) \
  for((node) = adm_cluster_first_node_at(0); \
      (node); \
      (node) = adm_cluster_first_node_at((node)->id + 1) \
  )

#define adm_cluster_for_each_param(param) \
  for((param) = &adm_cluster.params[0]; \
      ((param) < &adm_cluster.params[NBMAX_PARAMS]) && ((param)->name[0] != '\0'); \
      (param)++ \
  )


#endif /* __ADM_CLUSTER_H */
