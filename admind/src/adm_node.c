/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_node.h"

#include "os/include/os_error.h"
#include "os/include/os_string.h"
#include "os/include/os_thread.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_nic.h"
#include "common/include/exa_error.h"
#include "os/include/os_mem.h"
#include "log/include/log.h"

static void adm_node_remove_nic(struct adm_node *node);

/* Protect disk enumeration in disk check thread against disk removal. */
static os_thread_mutex_t disk_removal_lock;

void adm_node_static_init(void)
{
    os_thread_mutex_init(&disk_removal_lock);
}

void adm_node_lock_disk_removal()
{
    os_thread_mutex_lock(&disk_removal_lock);
}


void adm_node_unlock_disk_removal()
{
    os_thread_mutex_unlock(&disk_removal_lock);
}

/* node */

struct adm_node *
adm_node_alloc(void)
{
  struct adm_node *node;

  node = os_malloc(sizeof(struct adm_node));
  if (node == NULL)
    return NULL;

  memset(node, 0, sizeof(struct adm_node));

  node->spof_id = SPOF_ID_NONE;

  return node;
}


void
adm_node_delete(struct adm_node *node)
{
  struct adm_disk *disk;
  struct adm_nic *nic = node->nic;

  if (nic != NULL)
  {
      adm_node_remove_nic(node);
      adm_nic_free(nic);
  }

  while ((disk = node->disks) != NULL)
  {
    adm_disk_delete(disk);
  }

  os_free(node);
}


/* disks */

int
adm_node_insert_disk(struct adm_node *node, struct adm_disk *new)
{
  struct adm_disk *curr;
  struct adm_disk *prev;

  EXA_ASSERT(node != NULL);
  EXA_ASSERT(new != NULL);
  EXA_ASSERT(new->node_id != EXA_NODEID_NONE);
  EXA_ASSERT(new->next_in_node == NULL);

  if (node->nb_disks == NBMAX_DISKS_PER_NODE)
    return -ADMIND_ERR_TOO_MANY_DISKS_IN_NODE;

  if (adm_node_get_disk_by_uuid(node, &new->uuid) != NULL)
    return -EEXIST;

  curr = node->disks;
  prev = NULL;

  while (curr && uuid_compare(&new->uuid, &curr->uuid) < 0)
  {
    prev = curr;
    curr = curr->next_in_node;
  }

  if (curr && uuid_compare(&new->uuid, &curr->uuid) == 0)
    return -EEXIST;

  if (curr != NULL)
    new->next_in_node = curr;

  if (prev == NULL)
    node->disks = new;
  else
    prev->next_in_node = new;

  node->nb_disks++;

  return EXA_SUCCESS;
}


void
adm_node_remove_disk(struct adm_node *node, struct adm_disk *old)
{
  struct adm_disk *disk;
  struct adm_disk *prev;

  adm_node_lock_disk_removal();

  EXA_ASSERT(old != NULL);
  EXA_ASSERT(node != NULL && node->id == old->node_id);

  node->nb_disks--;

  disk = node->disks;
  prev = NULL;

  while (disk && disk != old)
  {
    prev = disk;
    disk = disk->next_in_node;
  }

  EXA_ASSERT(disk == old);

  if (prev == NULL)
    node->disks = old->next_in_node;
  else
    prev->next_in_node = old->next_in_node;

  old->next_in_node = NULL;
  old->node_id = EXA_NODEID_NONE;

  adm_node_unlock_disk_removal();
}


unsigned int
adm_node_nb_disks(const struct adm_node *node)
{
  return node->nb_disks;
}


struct adm_disk *
adm_node_get_disk_by_path(const struct adm_node *node, const char *path)
{
  struct adm_disk *disk;

  EXA_ASSERT(node != NULL);
  EXA_ASSERT(path != NULL);

  adm_node_for_each_disk(node, disk)
  {
    if (!strncmp(disk->path, path, EXA_MAXSIZE_DEVPATH + 1))
      return disk;
  }

  return NULL;
}


struct adm_disk *
adm_node_get_disk_by_uuid(const struct adm_node *node, const exa_uuid_t *uuid)
{
  struct adm_disk *disk;

  EXA_ASSERT(node != NULL);
  EXA_ASSERT(uuid != NULL);

  adm_node_for_each_disk(node, disk)
  {
    if (uuid_is_equal(&disk->uuid, uuid))
      return disk;
  }

  return NULL;
}


/* nics */

int
adm_node_insert_nic(struct adm_node *node, struct adm_nic *nic)
{
  EXA_ASSERT(node != NULL);
  EXA_ASSERT(nic != NULL);

  if (node->nic != NULL)
      return -EEXIST;

  node->nic = nic;

  return EXA_SUCCESS;
}


static void adm_node_remove_nic(struct adm_node *node)
{
  EXA_ASSERT(node->nic != NULL);
  node->nic = NULL;
}

struct adm_nic *adm_node_get_nic(const struct adm_node *node)
{
    if (node == NULL)
	return NULL;

   return node->nic;
}

void adm_node_set_spof_id(struct adm_node *node, spof_id_t spof_id)
{
    EXA_ASSERT(node != NULL);
    node->spof_id = spof_id;
}

spof_id_t adm_node_get_spof_id(const struct adm_node *node)
{
    EXA_ASSERT(node != NULL);
    return node->spof_id;
}

spof_id_t adm_node_get_first_free_spof_id(void)
{
    struct adm_node *node;
    spof_id_t max_spof_id = SPOF_ID_NONE;

    adm_cluster_for_each_node(node)
    {
        if (node->spof_id > max_spof_id)
            max_spof_id = node->spof_id;
    }

    if (max_spof_id == SPOF_ID_NONE)
        return 1;

    return max_spof_id + 1;
}
