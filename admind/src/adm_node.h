/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_NODE_H
#define __ADM_NODE_H

#include "os/include/os_network.h"
#include "common/include/uuid.h"
#include "common/include/exa_nodeset.h"
#include "vrt/common/include/spof.h"

#include "admind/src/adm_nic.h"

struct adm_disk;

struct adm_node
{
  exa_nodeid_t id;
  char name[EXA_MAXSIZE_NODENAME + 1];
  /* FIXME should hostname be a part of adm_nic field ? */
  char hostname[EXA_MAXSIZE_HOSTNAME + 1];
  struct adm_nic *nic;
  struct adm_disk *disks;
  unsigned int nb_disks;
  int managed_by_serverd;
  int managed_by_clientd;
  spof_id_t spof_id;
};


/* Functions */

void adm_node_static_init(void);

struct adm_node *adm_node_alloc(void);
void adm_node_delete(struct adm_node *node);

int adm_node_insert_disk(struct adm_node *node, struct adm_disk *disk);
void adm_node_remove_disk(struct adm_node *node, struct adm_disk *disk);
void adm_node_lock_disk_removal();
void adm_node_unlock_disk_removal();
unsigned int adm_node_nb_disks(struct adm_node *node);
struct adm_disk *adm_node_get_disk_by_path(struct adm_node *node, const char *path);
struct adm_disk *adm_node_get_disk_by_uuid(struct adm_node *node, const exa_uuid_t *uuid);

int adm_node_insert_nic(struct adm_node *node, struct adm_nic *nic);

struct adm_nic *adm_node_get_nic_by_name(struct adm_node *node, const char *name);
struct adm_nic *adm_node_first_nic_at(struct adm_node *node, int index);
struct adm_nic *adm_node_get_nic(struct adm_node *node);

void adm_node_set_spof_id(struct adm_node *node, spof_id_t spof_id);
spof_id_t adm_node_get_spof_id(const struct adm_node *node);
spof_id_t adm_node_get_first_free_spof_id(void);

/* Macros */

#define adm_node_for_each_disk(node, disk) \
  for(disk = node->disks; \
      disk != NULL; \
      disk = disk->next_in_node \
  )

#define adm_node_for_each_nic(node, nic) \
  for(nic = adm_node_first_nic_at(node, 0); \
      nic != NULL; \
      nic = adm_node_first_nic_at(node, nic->index_in_node + 1) \
  )

#endif /* __ADM_NODE_H */
