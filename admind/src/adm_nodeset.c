/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_nodeset.h"

#include <stdlib.h>
#include "os/include/os_string.h"
#include "os/include/os_error.h"
#include "os/include/os_stdio.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_config.h"
#include "os/include/strlcpy.h"
#include "common/include/exa_error.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"


/* --- adm_nodeset_contains_me ------------------------------------ */

/**
 * Return true if the local node is in a node set.
 */
int
adm_nodeset_contains_me(const exa_nodeset_t *set)
{
  return exa_nodeset_contains(set, adm_my_id);
}


/* --- adm_nodeid_to_name ----------------------------------------- */

/**
 * Convert a node id (number) to a node name (string).
 * Note: the returned string has the const qualifier.
 */
const char *
adm_nodeid_to_name(exa_nodeid_t id)
{
  struct adm_node *node;

  node = adm_cluster_get_node_by_id(id);
  EXA_ASSERT(node);

  return node->name;
}


/* --- adm_node_name_to_id ---------------------------------------- */

/**
 * Convert a node name (string) to a node id (number).
 */
exa_nodeid_t
adm_nodeid_from_name(const char *name)
{
  struct adm_node *node;

  adm_cluster_lock();

  /* EXAMSG_LOCALHOST is defined as an empty string */
  if (*name == 0)
    node = adm_myself();
  else
    node = adm_cluster_get_node_by_name(name);
  EXA_ASSERT(node);

  adm_cluster_unlock();

  EXA_ASSERT(node->id < EXA_MAX_NODES_NUMBER);

  return node->id;
}


/* --- adm_nodeset_set_all --------------------------------------- */

/**
 * Set a node set to all nodes in the cluster.
 */
void adm_nodeset_set_all(exa_nodeset_t *set)
{
  struct adm_node *node;

  exa_nodeset_reset(set);
  adm_cluster_for_each_node(node)
    exa_nodeset_add(set, node->id);
}


/* --- adm_nodeset_from_names ------------------------------------ */

/**
 * Initialise a node set from a space separated list of nodes.
 */
int
adm_nodeset_from_names(exa_nodeset_t *set, const char *nodelist)
{
  char buf[EXA_MAXSIZE_HOSTSLIST + 1];
  char *bufp = buf;
  const char *node_name;

  strlcpy(buf, nodelist, sizeof(buf));
  exa_nodeset_reset(set);

  adm_cluster_lock();

  while((node_name = os_strtok(buf == bufp ? buf : NULL, " ", &bufp)))
  {
    struct adm_node *node;

    /* And so empty tokens are ignored */
    if (node_name[0] == '\0')
        continue;

    node = adm_cluster_get_node_by_name(node_name);
    if (node == NULL)
    {
      adm_cluster_unlock();
      return -ADMIND_ERR_UNKNOWN_NODENAME;
    }
    exa_nodeset_add(set, node->id);
  }

  adm_cluster_unlock();

  return EXA_SUCCESS;
}


/* --- adm_nodeset_to_names --------------------------------------- */

/**
 * Fill a string with the list of the nodes that belong to a node set.
 */
void
adm_nodeset_to_names(const exa_nodeset_t *set, char *list, size_t n)
{
  exa_nodeid_t node;
  int done = 0;

  list[0]='\0';
  exa_nodeset_foreach(set, node)
  {
    done += os_snprintf(list + done, n - done, done == 0 ? "%s" : " %s",
		     adm_nodeid_to_name(node));
    if (done > n)
      break;
  }
}

int xml_set_prop_nodeset(xmlNodePtr xml_node, const char *name,
			 const exa_nodeset_t *set)
{
  char str[EXA_MAXSIZE_HOSTSLIST + 1];

  adm_nodeset_to_names(set, str, sizeof(str));

  return xml_set_prop(xml_node, name, str);
}
