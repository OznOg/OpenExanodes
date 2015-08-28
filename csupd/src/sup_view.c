/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "sup_view.h"
#include "sup_debug.h"

#include <string.h>

/**
 * Get the name of a state.
 *
 * \param[in] state  State to get the name of
 *
 * \return State name or NULL if invalid
 */
const char *
sup_state_name(sup_state_t state)
{
  switch (state)
    {
    case SUP_STATE_UNKNOWN:
      return "UNKNOWN";
    case SUP_STATE_CHANGE:
      return "CHANGE";
    case SUP_STATE_ACCEPT:
      return "ACCEPT";
    case SUP_STATE_COMMIT:
      return "COMMIT";
    }

  return NULL;
}

/**
 * Initialize a view.
 */
void
sup_view_init(sup_view_t *view)
{
  EXA_ASSERT(view);

  view->state = SUP_STATE_UNKNOWN;

  exa_nodeset_reset(&view->nodes_seen);
  view->num_seen = 0;

  exa_nodeset_reset(&view->clique);

  view->coord = EXA_NODEID_NONE;

  view->accepted = 0;
  view->committed = 0;
}

/**
 * Add a node to a view's seen nodes.
 *
 * \param     view     View to add a node to
 * \param[in] node_id  Id of node to add
 */
void
sup_view_add_node(sup_view_t *view, exa_nodeid_t node_id)
{
  EXA_ASSERT(view);

  exa_nodeset_add(&view->nodes_seen, node_id);
  if (node_id >= view->num_seen)
    view->num_seen = node_id + 1;
}

/**
 * Remove a node from a view's seen nodes.
 *
 * \param     view     View to remove a node from
 * \param[in] node_id  Id of node to remove
 */
void
sup_view_del_node(sup_view_t *view, exa_nodeid_t node_id)
{
  EXA_ASSERT(view);

  exa_nodeset_del(&view->nodes_seen, node_id);
  if (node_id == view->num_seen - 1)
    {
      while (node_id > 0 && !exa_nodeset_contains(&view->nodes_seen, node_id))
	node_id--;

      if (exa_nodeset_contains(&view->nodes_seen, node_id))
	view->num_seen = node_id + 1;
      else
	view->num_seen = 0;
    }
}

/**
 * Copy a view to another.
 *
 * \param[out] dest  View copied to
 * \param[in]  src   View copied from
 */
void
sup_view_copy(sup_view_t *dest, const sup_view_t *src)
{
  EXA_ASSERT(dest && src);
  memcpy(dest, src, sizeof(*dest));
}

/**
 * Check equality of two views.
 *
 * Two views are equal if both their nodes seen and
 * their coordinators are equal.
 *
 * \param[in] v1  View
 * \param[in] v2  View
 *
 * \return true if views are equal, false otherwise
 */
bool
sup_view_equals(const sup_view_t *v1, const sup_view_t *v2)
{
  EXA_ASSERT(v1 && v2);

  return (v1->coord == v2->coord
	  && exa_nodeset_equals(&v1->nodes_seen, &v2->nodes_seen));
}

/**
 * Print a view in a file.
 *
 * \param[in] view       View to print
 * \param     file       File to print to
 */
void
sup_view_debug(const sup_view_t *view)
{
  char seen_str[EXA_MAX_NODES_NUMBER + 1];
  char clique_str[EXA_MAX_NODES_NUMBER + 1];

  if (view == NULL)
    {
      __debug("(null view)");
      return;
    }

  exa_nodeset_to_bin(&view->nodes_seen, seen_str);
  exa_nodeset_to_bin(&view->clique, clique_str);

  __debug("state=%s seen=%s num_seen=%u clique=%s coord=%u"
	  " accepted=%u committed=%u", sup_state_name(view->state),
	  seen_str, view->num_seen, clique_str, view->coord,
	  view->accepted, view->committed);
}
