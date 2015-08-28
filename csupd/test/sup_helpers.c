/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "sup_helpers.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "os/include/os_error.h"
#include "os/include/os_string.h"
#include "os/include/os_file.h"

/**
 * Parse a string and get the contained membership.
 *
 * \param[out] mship  Membership parsed
 * \param[in]  str    String to parse
 *
 * \return 0 if successfull, -EINVAL if invalid string
 */
static int
mship_from_str(exa_nodeset_t *mship, const char *str)
{
  exa_nodeid_t node_id;
  int len = (int)strlen(str);
  int i;

  if (len == 0)
    return -EINVAL;

  exa_nodeset_reset(mship);

  node_id = 0;
  for (i = len - 1; i >= 0; i--)
    {
      if (str[i] == '1')
	exa_nodeset_add(mship, node_id);
      else if (str[i] != '0')
	return -EINVAL;

      node_id++;
    }

  return 0;
}

/**
 * Parse a node number and a membership from a string.
 *
 * The syntax is <node_id> : <space> <mship>
 *
 * XXX Should distinguish cases currently reported as -EINVAL.
 *
 * \param[in,out] str      String to parse
 * \param[out]    node_id  Node id parsed
 * \param[out]    mship    Membership parsed
 *
 * \return 0 if successfull, negative error code otherwise
 */
static int
parse_mship(char *str, exa_nodeid_t *node_id, exa_nodeset_t *mship)
{
  char *p, *dummy;

  EXA_ASSERT(str);
  EXA_ASSERT(node_id && mship);

  /* Get node number */
  p = os_strtok(str, " ", &dummy);
  if (p == NULL)
    return -EINVAL;

  if (sscanf(p, "%u:", node_id) != 1)
    return -EINVAL;

  /* Get mship */
  p = os_strtok(NULL, " ", &dummy);
  if (p == NULL)
    return -EINVAL;

  if (strlen(p) > EXA_MAX_NODES_NUMBER)
    return -E2BIG;

  if (mship_from_str(mship, p) < 0)
    return -EINVAL;

  /* A node *must* be present in its own membership */
  if (!exa_nodeset_contains(mship, *node_id))
    return -EILSEQ;

  /* For now, just ignore any remaining stuff in the string */

  return 0;
}

/**
 * Parse an expected membership from a string.
 *
 * The syntax is <node> <space> => <space> <expected mship>
 *
 * \param[in,out] str       String to parse
 * \param[out]    node_id   Id of node for which the mship is expected
 * \param[out]    expected  Expected mship parsed
 *
 * \return 0 if successfull, negative error code otherwise
 */
static int
parse_expected(char *str, exa_nodeid_t *node_id, exa_nodeset_t *expected)
{
  char *p, *dummy;

  EXA_ASSERT(str);
  EXA_ASSERT(expected);

  /* Get node id */
  p = os_strtok(str, " ", &dummy);
  if (p == NULL)
    return -EINVAL;

  if (sscanf(p, "%u", node_id) != 1)
    return -EINVAL;

  /* Eat up '=>' */
  p = os_strtok(NULL, " ", &dummy);
  if (p == NULL || strcmp(p, "=>"))
    return -EINVAL;

  /* Get membership */
  p = os_strtok(NULL, " ", &dummy);
  if (p == NULL)
    return -EINVAL;

  if (strlen(p) > EXA_MAX_NODES_NUMBER)
    return -E2BIG;

  if (mship_from_str(expected, p) < 0)
    return -EINVAL;

  /* A node *must* be present in the mship it expects */
  if (!exa_nodeset_contains(expected, *node_id))
    return -EILSEQ;

  /* For now, just ignore any remaining stuff in the string */

  return 0;
}

/**
 * Trim space at the end of the line.
 *
 * \param[in,out] line  Line to trim
 */
static void
rtrim(char *line)
{
  size_t len = strlen(line);
  while (len > 0 && isspace(line[len - 1]))
    len--;

  line[len] = '\0';
}

/**
 * Tell whether a line should be ignored.
 *
 * \param[in] line  Line to check
 *
 * \return true if line is empty or is a comment, false otherwise
 */
static bool
ignore_line(const char *line)
{
  int i;
  bool first = true;

  for (i = 0; line[i]; i++)
    {
      if (line[i] == '#' && first)
	return true;

      if (!isspace(line[i]))
	return false;

      first = false;
    }

  return true;
}

exa_nodeset_t *mships = NULL;
exa_nodeset_t *exps = NULL;

/* Reallocate both the 'viewed mships' and 'expected mships' arrays */
void realloc_mships_and_exps(unsigned *num_nodes, int n)
{
    mships = realloc(mships, n * sizeof(exa_nodeset_t));
    exps = realloc(exps, n * sizeof(exa_nodeset_t));
    *num_nodes = n;
}

/* Set the viewed mship of a node; error if already defined */
static int set_mship(unsigned *num_nodes, exa_nodeset_t *mships_defined,
	             exa_nodeid_t node_id, const exa_nodeset_t *mship)
{
    if (exa_nodeset_contains(mships_defined, node_id))
	return -EEXIST;

    exa_nodeset_add(mships_defined, node_id);

    if (mships == NULL || node_id >= *num_nodes)
	realloc_mships_and_exps(num_nodes, *num_nodes + 1);

    exa_nodeset_copy(&mships[node_id], mship);

    return 0;
}

/* Set the expected mship of a node; error if already defined */
static int set_exp(unsigned *num_nodes, exa_nodeset_t *exps_defined,
	           exa_nodeid_t node_id, const exa_nodeset_t *exp)
{
    if (exa_nodeset_contains(exps_defined, node_id))
	return -EEXIST;

    exa_nodeset_add(exps_defined, node_id);

    if (exps == NULL || node_id >= *num_nodes)
	realloc_mships_and_exps(num_nodes, *num_nodes + 1);

    exa_nodeset_copy(&exps[node_id], exp);

    return 0;
}


/**
 * Read a set of <node id, membership> from a file.
 *
 * \param         file          File to read from
 * \param[in,out] num_nodes     Number of nodes read
 * \param[out]    mship_tab     Table of memberships read
 * \param[out]    expected_tab  Expected resulting memberships
 *
 * #expected_tab is optional. If non-NULL, it will contain the
 * expected results of the membership calculation, on a per-node basis.
 *
 * The function will return an error if #expected_tab is non-NULL but
 * not all nodes have an expected membership in the file.
 *
 * The caller is responsible for freeing #mship_tab and #expected_tab.
 *
 * \return 0 if successfull, negative error code otherwise
 */
int
read_mship_set(FILE *file, unsigned *num_nodes, exa_nodeset_t **mship_tab,
	       exa_nodeset_t **expected_tab)
{
  exa_nodeset_t mships_defined;
  exa_nodeset_t exps_defined;
  bool in_expected = false;
  int err = 0;

  mships = NULL;
  exps = NULL;

  EXA_ASSERT(file);
  EXA_ASSERT(mship_tab);

  *num_nodes = 0;
  *mship_tab = NULL;
  if (expected_tab)
    *expected_tab = NULL;

  exa_nodeset_reset(&mships_defined);
  exa_nodeset_reset(&exps_defined);

  while (1)
    {
      /* Arbitrary size, just ensures there is enough room for a full mship,
	 a node id, and possibly optional data */
      char line[EXA_MAX_NODES_NUMBER * 2];

      do
	{
	  if (fgets(line, sizeof(line), file) == NULL)
	    goto end;
	  rtrim(line);
	}
      while (ignore_line(line));

      if (strcmp(line, "expected:") == 0)
	{
	  in_expected = true;
	}
      else if (in_expected)
	{
	  exa_nodeid_t node_id;
	  exa_nodeset_t exp;

	  err = parse_expected(line, &node_id, &exp);
	  if (err < 0)
	    goto end;

	  err = set_exp(num_nodes, &exps_defined, node_id, &exp);
	  if (err < 0)
	    goto end;
	}
      else
	{
	  exa_nodeid_t node_id;
	  exa_nodeset_t mship;

	  err = parse_mship(line, &node_id, &mship);
	  if (err < 0)
	    goto end;

	  err = set_mship(num_nodes, &mships_defined, node_id, &mship);
	  if (err < 0)
	    goto end;
	}
    }

 end:

  /* Error if expected memberships were requested and not all nodes
   * had one defined */
  if (!err && expected_tab &&
      exa_nodeset_count(&exps_defined) != exa_nodeset_count(&mships_defined))
    err = -ESRCH;

  if (!err && expected_tab)
  {
      exa_nodeid_t node_id;

      /* check if a node can actually have the requested membership given the
       * mship it sees */
      for (node_id = 0; node_id < *num_nodes; node_id++)
      {
	  exa_nodeset_t intersec;
	  exa_nodeset_copy(&intersec, &exps[node_id]);
	  exa_nodeset_intersect(&intersec, &mships[node_id]);
	  if (!exa_nodeset_equals(&intersec, &exps[node_id]))
	      err = -EPROTO;
      }
  }


  if (err)
    {
      free(mships);
      free(exps);
    }
  else
    {
      *mship_tab = mships;
      if (expected_tab)
	*expected_tab = exps;
      else
	free(exps);
    }

  return err;
}

/**
 * Read a cluster from a file.
 *
 * \param      file      File to read the cluster from
 * \param[out] cluster   Cluster built
 * \param[out] expected  Expected calculated membership, per node (may be NULL)
 *
 * Notes:
 *   - #cluster->self is *not* set;
 *   - the caller is responsible for freeing #expected.
 *
 * \return 0 if successfull, negative error code otherwise
 */
int
read_cluster(FILE *file, sup_cluster_t *cluster, exa_nodeset_t **expected)
{
  unsigned num_nodes;
  exa_nodeset_t *mship_tab;
  exa_nodeid_t node_id;
  int err;

  err = read_mship_set(file, &num_nodes, &mship_tab, expected);
  if (err < 0)
    return err;

  sup_cluster_init(cluster);

  for (node_id = 0; node_id < num_nodes; node_id++)
    {
      sup_node_t *node;

      err = sup_cluster_add_node(cluster, node_id);
      if (err < 0)
	goto end;

      node = sup_cluster_node(cluster, node_id);
      exa_nodeset_copy(&node->view.nodes_seen, &mship_tab[node_id]);
    }

 end:

  if (err)
    {
      free(mship_tab);
      free(*expected);
    }

  return err;
}

/**
 * Open a cluster file.
 *
 * \param[in]  filename  Name of cluster file
 * \param[out] cluster   Cluster built
 * \param[out] expected  Expected calculated membership, per node (may be NULL)
 *
 * The caller is responsible for freeing #expected.
 *
 * \return 0 if successfull, negative error code otherwise
 */
int
open_cluster(const char *filename, sup_cluster_t *cluster,
	     exa_nodeset_t **expected)
{
  const char *srcdir = getenv("srcdir");
  char path[512];
  FILE *f;
  int r;

  sprintf(path, "%s" OS_FILE_SEP "mship_files" OS_FILE_SEP "%s",
          srcdir ? srcdir : "." , filename);

  f = fopen(path, "rt");
  if (f == NULL)
    return -errno;

  r = read_cluster(f, cluster, expected);
  fclose(f);

  return r;
}
