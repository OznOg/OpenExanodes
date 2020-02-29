/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_cluster.h"

#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_hostname.h"
#include "admind/src/adm_atomic_file.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"
#include "os/include/os_thread.h"
#include "os/include/os_stdio.h"

/* XXX In fact this is *not* a cluster goal. All goal-related stuff should
 * eventually be moved out to adm_goal.[ch] or something... */
#define ADMIND_CLUSTER_GOAL_FILE "node.goal"

struct adm_cluster adm_cluster;
char *adm_config_buffer = NULL;
int adm_config_size = 0;

exa_nodeid_t adm_my_id = EXA_NODEID_NONE;
exa_nodeid_t adm_leader_id = EXA_NODEID_NONE;
bool adm_leader_set = false;

static os_thread_mutex_t adm_cluster_mutex;
static struct adm_node *myself = NULL;

/** \brief Return true if the local node is leader, false else
 *
 * \return true or false
 */
bool
adm_is_leader(void)
{
  return adm_leader_set && adm_leader_id == adm_my_id;
}

/* nodes */

int
adm_cluster_insert_node(struct adm_node *node)
{
  EXA_ASSERT(node != NULL);
  EXA_ASSERT(node->id < EXA_MAX_NODES_NUMBER);

  if (adm_cluster_get_node_by_name(node->name) != NULL)
    return -EEXIST;

  if (adm_cluster.nodes[node->id] != NULL)
    return -EEXIST;

  adm_cluster.nodes[node->id] = node;
  adm_cluster.nodes_number++;

  if (myself == NULL &&
      strncmp(node->hostname, adm_hostname(), EXA_MAXSIZE_HOSTNAME) == 0)
  {
    exalog_debug("found hostname '%s': set myself='%s' (%u)",
		 adm_hostname(), node->name, node->id);
    myself = node;
    adm_my_id = node->id;
  }

  return EXA_SUCCESS;
}


struct adm_node *
adm_cluster_remove_node(const struct adm_node *node)
{
  struct adm_disk *disk;
  struct adm_node *former;
  EXA_ASSERT(node != NULL);
  EXA_ASSERT(adm_cluster.nodes[node->id] == node);

  former = adm_cluster.nodes[node->id];
  adm_cluster.nodes[node->id] = NULL;
  adm_cluster.nodes_number--;

  while ((disk = node->disks) != NULL)
  {
    adm_node_remove_disk(former, disk);
  }

  return former;
}


unsigned int
adm_cluster_nb_nodes(void)
{
  return adm_cluster.nodes_number;
}


static struct adm_node *_adm_cluster_get_node_by_id(exa_nodeid_t id)
{
  EXA_ASSERT(id < EXA_MAX_NODES_NUMBER);

  return adm_cluster.nodes[id];
}

const struct adm_node *
adm_cluster_get_node_by_id(exa_nodeid_t id)
{
  return _adm_cluster_get_node_by_id(id);
}


const struct adm_node *
adm_cluster_get_node_by_name(const char *name)
{
  exa_nodeid_t id;

  EXA_ASSERT(name != NULL);

  for (id = 0; id < EXA_MAX_NODES_NUMBER; id++)
  {
    if (adm_cluster.nodes[id] != NULL &&
	!strncmp(adm_cluster.nodes[id]->name, name, EXA_MAXSIZE_NODENAME + 1))
      return adm_cluster.nodes[id];
  }

  return NULL;
}


struct adm_node *
adm_cluster_first_node_at(exa_nodeid_t id)
{
  while (id < EXA_MAX_NODES_NUMBER && adm_cluster.nodes[id] == NULL)
    id++;

  EXA_ASSERT(id <= EXA_MAX_NODES_NUMBER);
  if (id >= EXA_MAX_NODES_NUMBER)
    return NULL;

  return adm_cluster.nodes[id];
}

/* cluster */

static void
adm_cluster_init_params(void)
{
  exa_service_parameter_t *exa_param;
  int i, j;

  for (i = 0; i < NBMAX_PARAMS; i++)
    memset(&adm_cluster.params[i], 0, sizeof(adm_cluster.params[i]));

  i = 0;
  j = 0;
  while((exa_param = exa_service_parameter_get_list(&i)) != NULL)
  {
    EXA_ASSERT(j < NBMAX_PARAMS);
    strlcpy(adm_cluster.params[j].name, exa_param->name,
            sizeof(adm_cluster.params[j].name));
    strlcpy(adm_cluster.params[j].default_value, exa_param->default_value,
            sizeof(adm_cluster.params[j].default_value));
    j++;
  }
}


void
adm_cluster_init(void)
{
  os_thread_mutex_init(&adm_cluster_mutex);

  memset(&adm_cluster, 0, sizeof(adm_cluster));

  adm_cluster.version = -1;
  adm_cluster.created = false;

  adm_cluster_init_params();

  /* FIXME This call is made necessary because of the dependency between
   * nodes and disk (actually this call perform a mutex initialization).
   * In a perfect world, such a dependency would not exist, as there is
   * no need to have a strong link between a nodelist and the local list
   * of rdevs. There could be some kind of 'soft' link by using a nodename
   * or node id to query info about disk, no more pointer with makes
   * locking necessary... */
  adm_node_static_init();
}


void
adm_cluster_cleanup(void)
{
  int i;
  struct adm_group *group;

  /* Remove all groups in the linked list */
  while ((group = adm_group_get_first()) != NULL)
  {
      adm_group_remove_group(group);

      adm_group_cleanup_group(group);

      if (group->sb_version)
          sb_version_unload(group->sb_version);

      adm_group_free(group);
  }

  for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
  {
    struct adm_node *node = adm_cluster.nodes[i];

    if (node == NULL)
      continue;

    adm_cluster_remove_node(node);
    adm_node_delete(node);
  }

  adm_cluster.version = -1;
  adm_cluster.created = false;
  myself = NULL;

  adm_cluster_init_params();
}


int
adm_cluster_insert_disk(struct adm_disk *disk)
{
  struct adm_node *node;
  if (adm_cluster.disks_number == NBMAX_DISKS)
    return -ADMIND_ERR_TOO_MANY_DISKS;

  adm_cluster.disks_number++;

  node = _adm_cluster_get_node_by_id(disk->node_id);
  return adm_node_insert_disk(node, disk);
}


struct adm_disk *adm_cluster_remove_disk(const exa_uuid_t *disk_uuid)
{
  struct adm_node *node;
  struct adm_disk *disk = adm_cluster_get_disk_by_uuid(disk_uuid);
  if (disk == NULL)
      return NULL;

  node = _adm_cluster_get_node_by_id(disk->node_id);
  EXA_ASSERT(node != NULL);

  adm_node_remove_disk(node, disk);
  adm_cluster.disks_number--;

  return disk;
}


unsigned int
adm_cluster_nb_disks(void)
{
  return adm_cluster.disks_number;
}


struct adm_disk *
adm_cluster_get_disk_by_path(const char *node_name, const char *disk_path)
{
  const struct adm_node *node;

  node = adm_cluster_get_node_by_name(node_name);
  if (!node)
    return NULL;

  return adm_node_get_disk_by_path(node, disk_path);
}


struct adm_disk *
adm_cluster_get_disk_by_uuid(const exa_uuid_t *uuid)
{
  const struct adm_node *node;

  adm_cluster_for_each_node(node)
  {
    struct adm_disk *disk;

    disk = adm_node_get_disk_by_uuid(node, uuid);
    if (disk)
      return disk;
  }

  return NULL;
}


struct adm_volume *
adm_cluster_get_volume_by_name(const char *group_name, const char *volume_name)
{
  struct adm_group *group;

  group = adm_group_get_group_by_name(group_name);
  if (!group)
    return NULL;

  return adm_group_get_volume_by_name(group, volume_name);
}


struct adm_volume *
adm_cluster_get_volume_by_uuid(const exa_uuid_t *uuid)
{
  struct adm_group *group;

  adm_group_for_each_group(group)
  {
    struct adm_volume *volume;

    volume = adm_group_get_volume_by_uuid(group, uuid);

    if (volume)
      return volume;
  }

  return NULL;
}

static struct adm_cluster_param *
adm_cluster_param_find(const char *name)
{
  int i;

  for (i = 0; i < NBMAX_PARAMS; i++)
    if (!strncmp(adm_cluster.params[i].name, name, EXA_MAXSIZE_PARAM_NAME))
      return &adm_cluster.params[i];

  return NULL;
}


int
adm_cluster_set_param(const char *name, const char *value)
{
  exa_service_parameter_t *exa_param;
  struct adm_cluster_param *adm_param;
  int ret;

  exa_param = exa_service_parameter_get(name);
  if (!exa_param)
    return -EXA_ERR_SERVICE_PARAM_UNKNOWN;

  ret = exa_service_parameter_check(exa_param, value);
  if (ret != EXA_SUCCESS)
    return ret;

  adm_param = adm_cluster_param_find(name);
  if (!adm_param)
    return -EXA_ERR_SERVICE_PARAM_UNKNOWN;

  strlcpy(adm_param->value, value, sizeof(adm_param->value));
  adm_param->set = true;

  return EXA_SUCCESS;
}


int
adm_cluster_set_param_default(const char *name, const char *default_value)
{
  exa_service_parameter_t *exa_param;
  struct adm_cluster_param *adm_param;
  int ret;

  exa_param = exa_service_parameter_get(name);
  if (!exa_param)
    return -EXA_ERR_SERVICE_PARAM_UNKNOWN;

  ret = exa_service_parameter_check(exa_param, default_value);
  if (ret != EXA_SUCCESS)
    return ret;

  adm_param = adm_cluster_param_find(name);
  if (!adm_param)
    return -EXA_ERR_SERVICE_PARAM_UNKNOWN;

  strlcpy(adm_param->default_value, default_value, sizeof(adm_param->default_value));

  return EXA_SUCCESS;
}


int
adm_cluster_set_param_to_default(const char *name)
{
  struct adm_cluster_param *adm_param;

  adm_param = adm_cluster_param_find(name);
  if (!adm_param)
    return -EXA_ERR_SERVICE_PARAM_UNKNOWN;

  adm_param->set = false;
  memset(adm_param->value, 0, sizeof(adm_param->value));

  return EXA_SUCCESS;
}


const char *
adm_cluster_get_param_default(const char *name)
{
  struct adm_cluster_param *adm_param;

  adm_param = adm_cluster_param_find(name);
  EXA_ASSERT_VERBOSE(adm_param, "param '%s' not found", name);

  return adm_param->default_value;
}


const char *
adm_cluster_get_param_text(const char *name)
{
  struct adm_cluster_param *adm_param;

  adm_param = adm_cluster_param_find(name);
  EXA_ASSERT_VERBOSE(adm_param, "param '%s' not found", name);

  if (adm_param->set)
    return adm_param->value;
  else
    return adm_param->default_value;
}


/* FIXME This function assumes the parameter is an integer. What if it's
         not? No error handling here... */
int
adm_cluster_get_param_int(const char *name)
{
  return atoi(adm_cluster_get_param_text(name));
}


int adm_cluster_get_param_boolean(const char *name)
{
    const char *param = adm_cluster_get_param_text(name);

    if (strcmp(param, ADMIND_PROP_TRUE) == 0)
        return true;
    else if (strcmp(param, ADMIND_PROP_FALSE) == 0)
        return false;
    else
        EXA_ASSERT_VERBOSE(false, "Invalid value '%s' for parameter '%s' in "
                           "configuration file.", param, name);
    return false;
}


/*
 * Warning, silently discard unknown node names. With dynamicity and user
 * supplied tunable values, it is possible to have a tunable parameter with
 * one or more unknown node name.
 *
 */
void
adm_cluster_get_param_nodeset(exa_nodeset_t *nodeset, const char *name)
{
  const char *value;

  value = adm_cluster_get_param_text(name);
  adm_nodeset_from_names(nodeset, value);

}


/**
 * Log with info level all tuning values that differs from the default value
 * hardcoded in service_parameter.c.
 */
void
adm_cluster_log_tuned_params()
{
    exa_service_parameter_t *exa_param;
    int i = 0;

    while((exa_param = exa_service_parameter_get_list(&i)) != NULL)
    {
        const char *value = adm_cluster_get_param_text(exa_param->name);
        if (strcmp(value, exa_param->default_value) != 0)
            exalog_info("Tuning: %s=%s", exa_param->name, value);
    }
}


const struct adm_node *
adm_myself(void)
{
  return myself;
}


const struct adm_node *
adm_leader(void)
{
  if (!adm_leader_set)
    return NULL;

  return adm_cluster_get_node_by_id(adm_leader_id);
}


void adm_cluster_lock(void)
{
    os_thread_mutex_lock(&adm_cluster_mutex);
}


void adm_cluster_unlock(void)
{
    os_thread_mutex_unlock(&adm_cluster_mutex);
}

static const char *adm_cluster_goal_to_str(adm_cluster_goal_t goal)
{
    switch (goal)
    {
    case ADM_CLUSTER_GOAL_UNDEFINED:
        return NULL;
    case ADM_CLUSTER_GOAL_STARTED:
        return "start";
    case ADM_CLUSTER_GOAL_STOPPED:
        return "stop";
    }

    return NULL;
}

static adm_cluster_goal_t adm_cluster_goal_from_str(const char *str)
{
    if (strcasecmp(str, "start") == 0)
        return ADM_CLUSTER_GOAL_STARTED;

    if (strcasecmp(str, "stop") == 0)
        return ADM_CLUSTER_GOAL_STOPPED;

    return ADM_CLUSTER_GOAL_UNDEFINED;
}

int adm_cluster_save_goal(adm_cluster_goal_t goal)
{
    char path[OS_PATH_MAX];
    const char *goal_str;
    char buf[8]; /* FIXME? */
    int n, err;

    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADMIND_CLUSTER_GOAL_FILE);

    goal_str = adm_cluster_goal_to_str(goal);
    EXA_ASSERT_VERBOSE(goal_str, "invalid persistent cluster goal: %d", goal);

    n = os_snprintf(buf, sizeof(buf), "%s\n", goal_str);
    EXA_ASSERT(n < sizeof(buf));

    err = adm_atomic_file_save(path, buf, strlen(buf));
    if (err)
        exalog_error("failed saving cluster goal file: %s", exa_error_msg(err));

    return err;
}

int adm_cluster_delete_goal(void)
{
    char path[OS_PATH_MAX];

    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADMIND_CLUSTER_GOAL_FILE);
    exalog_trace("deleting %s", path);

    return unlink(path) == -1 ? -errno : EXA_SUCCESS;
}

int adm_cluster_load_goal(adm_cluster_goal_t *goal)
{
    struct stat sb;
    int err = 0;
    FILE *goal_file = NULL;
    const char *error_msg = NULL;
    char goal_str[8]; /* FIXME? */
    char unexpected_str[4];  /* For dummy read, hence size doesn't matter */
    size_t len;
    char path[OS_PATH_MAX];

    *goal = ADM_CLUSTER_GOAL_UNDEFINED;

    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADMIND_CLUSTER_GOAL_FILE);
    exalog_debug("Loading node goal file '%s'", path);

    /* Check the file exists and is a regular file */
    if (stat(path, &sb) == -1)
    {
        if (errno == ENOENT)
        {
            exalog_info("No node goal file");
            return EXA_SUCCESS;
        }

        err = -errno;
        goto done;
    }

    if ((sb.st_mode & S_IFMT) != S_IFREG)
    {
        err = -EINVAL;
        error_msg = "node goal file is not a regular file";
        goto done;
    }

    goal_file = fopen(path, "rt");
    if (goal_file == NULL)
    {
        err = -errno;
        goto done;
    }

    if (fgets(goal_str, sizeof(goal_str), goal_file) == NULL)
    {
        err = -errno;
        goto done;
    }
    /* Remove trailing newline */
    len = strlen(goal_str);
    while (len > 0 && goal_str[len - 1] == '\n')
        len--;
    goal_str[len] = '\0';

    /* XXX Should we require all uppercase? */
    *goal = adm_cluster_goal_from_str(goal_str);
    if (*goal == ADM_CLUSTER_GOAL_UNDEFINED)
    {
        err = -EINVAL;
        error_msg = "invalid goal in node goal file";
        goto done;
    }

    /* Check there are no other lines in the file: the file is supposed to
     * contain a single line */
    if (fgets(unexpected_str, sizeof(unexpected_str), goal_file) != NULL)
    {
        err = -EINVAL;
        error_msg = "invalid node goal file";
        goto done;
    }


done:
    /* Close goal file if it's opened, and only return the possible close
     * error if there wasn't another one before
     */
    if (goal_file && fclose(goal_file) != 0 && err == 0)
        err = -errno;

    if (err)
    {
        if (error_msg == NULL)
            error_msg = exa_error_msg(err);
        exalog_error("failed reading node goal file: %s", error_msg);
    }

    return err;
}
