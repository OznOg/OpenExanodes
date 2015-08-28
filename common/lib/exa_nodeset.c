/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_nodeset.h"

#include "os/include/os_stdio.h"

const exa_nodeset_t EXA_NODESET_EMPTY = { .cells = { 0ull, 0ull } };
const exa_nodeset_t EXA_NODESET_FULL  = { .cells = { ~0ull, ~0ull } };

#define CHECK_NODE(node)						\
  EXA_ASSERT_VERBOSE(EXA_NODEID_VALID(node), "invalide node id: %d", node)

int exa_nodeid_from_str(exa_nodeid_t *node_id, const char *str)
{
    if (node_id == NULL || str == NULL)
        return -EINVAL;

    if (to_uint(str, node_id) != EXA_SUCCESS)
        return -EINVAL;

    if (!EXA_NODEID_VALID(*node_id))
        return -EINVAL;

    return EXA_SUCCESS;
}

const char *exa_nodeid_to_str(exa_nodeid_t node_id)
{
    /* Enough room for now, as we support 128 nodes max. */
    static __thread char str[3 + 1];

    if (!EXA_NODEID_VALID(node_id))
        return NULL;

    if (os_snprintf(str, sizeof(str), "%"PRInodeid, node_id) >= sizeof(str))
        return NULL;

    return str;
}

/**
 * Reset the nodeset
 */
void exa_nodeset_reset(exa_nodeset_t *set)
{
  *set = EXA_NODESET_EMPTY;
}

/**
 * Copy a nodes set to another.
 */
void exa_nodeset_copy(exa_nodeset_t *to, const exa_nodeset_t *from)
{
  *to = *from;
}

/**
 * Return true if and only if the node set does not contain any node.
 */
bool exa_nodeset_is_empty(const exa_nodeset_t *set)
{
  int i;
  for (i = 0; i < EXA_NODESET_NB_CELLS; i++)
    if (set->cells[i])
      return false;
  return true;
}

/**
 * Add a node to a set of nodes.
 * Adding localhost is forbidden.
 */
void exa_nodeset_add(exa_nodeset_t *set, exa_nodeid_t node)
{
  CHECK_NODE(node);
  set->cells[node / EXA_NODESET_BITS_PER_CELL] |=
      1ull << (node % EXA_NODESET_BITS_PER_CELL);
}

/**
 * Remove a node to a set of nodes.
 * Removing localhost is forbidden.
 */
void exa_nodeset_del(exa_nodeset_t *set, exa_nodeid_t node)
{
  CHECK_NODE(node);
  set->cells[node / EXA_NODESET_BITS_PER_CELL] &=
      ~(1ull << (node % EXA_NODESET_BITS_PER_CELL));
}

/**
 * Return true if and only if a node belongs to the set.
 * Localhost doesn't belong to any set.
 */
bool exa_nodeset_contains(const exa_nodeset_t *set, exa_nodeid_t node)
{
  if (node == EXA_NODEID_NONE || node == EXA_NODEID_LOCALHOST)
    return false;

  CHECK_NODE(node);
  return (set->cells[node / EXA_NODESET_BITS_PER_CELL] &
      1ull << (node % EXA_NODESET_BITS_PER_CELL))
      ?true:false;
}

/**
 * Return true iff two node sets are equal.
 */
bool exa_nodeset_equals(const exa_nodeset_t *set1, const exa_nodeset_t *set2)
{
  int i;

  for (i = 0; i < EXA_NODESET_NB_CELLS; i++)
    if (set1->cells[i] != set2->cells[i])
      return false;

  return true;
}

/**
 * Compute the union of two node sets.
 */
void exa_nodeset_sum(exa_nodeset_t *to, const exa_nodeset_t *from)
{
  int i;
  for (i = 0; i < EXA_NODESET_NB_CELLS; i++)
    to->cells[i] |= from->cells[i];
}

/**
 * Compute the substraction of two node sets.
 */
void exa_nodeset_substract(exa_nodeset_t *to, const exa_nodeset_t *from)
{
  int i;
  for (i = 0; i < EXA_NODESET_NB_CELLS; i++)
    to->cells[i] &= ~(from->cells[i]);
}

/**
 * Compute the intersection of two node sets.
 */
void exa_nodeset_intersect(exa_nodeset_t *to, const exa_nodeset_t *from)
{
  int i;
  for (i = 0; i < EXA_NODESET_NB_CELLS; i++)
    to->cells[i] &= from->cells[i];
}

/**
 * \brief Check if nodeset A is included in nodeset B
 *
 * \param[in] a the A nodeset
 * \param[in] b the B nodeset
 *
 * \return true if A is included in B, false otherwise
 */
bool exa_nodeset_included(const exa_nodeset_t *a,
		     const exa_nodeset_t *b)
{
  exa_nodeset_t set;

  exa_nodeset_copy(&set, a);
  exa_nodeset_intersect(&set, b);

  return exa_nodeset_equals(&set, a);
}

/**
 * Whether two nodesets are disjoint.
 *
 * @param[in] a  First nodeset
 * @param[in] b  Second nodeset
 *
 * @return true if a and b are disjoint, false otherwise
 */
bool exa_nodeset_disjoint(const exa_nodeset_t *a, const exa_nodeset_t *b)
{
    exa_nodeset_t set;

    exa_nodeset_copy(&set, a);
    exa_nodeset_intersect(&set, b);

    return exa_nodeset_is_empty(&set);
}

/**
 * Count the number of nodes belonging to a set.
 */
int exa_nodeset_count(const exa_nodeset_t *set)
{
  exa_nodeid_t node;
  int count = 0;
  for (node = 0; node < EXA_MAX_NODES_NUMBER; node++)
    if (exa_nodeset_contains(set, node))
      count++;
  return count;
}

/**
 * Get the number of significant bits to represent a node set.
 *
 * \param[in] set  Node set
 *
 * \return Number of significant bits
 */
unsigned exa_nodeset_num_bits(const exa_nodeset_t *set)
{
  exa_nodeset_iter_t iter;
  exa_nodeid_t node_id, max_node_id = 0;

  exa_nodeset_iter_init(set, &iter);
  while (exa_nodeset_iter(&iter, &node_id))
    max_node_id = node_id;

  return max_node_id + 1;
}

/**
 * Return the smallest id greather or equal to node of the nodes
 * belonging to a set.
 * If the set is empty, it returns EXA_MAX_NODES_NUMBER.
 */
exa_nodeid_t exa_nodeset_first_at(const exa_nodeset_t *set, exa_nodeid_t node)
{
  CHECK_NODE(node);
  while (node < EXA_MAX_NODES_NUMBER && !exa_nodeset_contains(set, node))
    node++;
  return node;
}

/**
 * Return the smallest id of the nodes belonging to a set.
 * If the set is empty, it returns EXA_MAX_NODES_NUMBER.
 */
exa_nodeid_t exa_nodeset_first(const exa_nodeset_t *set)
{
  return exa_nodeset_first_at(set, 0);
}

/**
 * Initialise an iterator with a node set
 */
void exa_nodeset_iter_init(const exa_nodeset_t *set, exa_nodeset_iter_t *iter)
{
  exa_nodeset_copy(iter, set);
}

/**
 * Give the next node in the iterator. Return false when there is
 * no more node to iterate.
 *
 * Usage:
 *   exa_nodeset_iter_t iterator;
 *   exa_nodeid_t node;
 *   exa_nodeset_iter_init(iterator, set);
 *   while(exa_nodeset_iter(iterator, node)
 *   {
 *     foo(node);
 *   }
 */
int exa_nodeset_iter(exa_nodeset_iter_t *iter, exa_nodeid_t *node)
{
  *node = exa_nodeset_first_at(iter, 0);
  if (*node >= EXA_MAX_NODES_NUMBER)
    return false;
  exa_nodeset_del(iter, *node);
  return true;
}

/**
 * Parse an hex string to initialise a node set.
 * The string should be EXA_NODESET_HEX_SIZE.
 */
int exa_nodeset_from_hex(exa_nodeset_t *set, const char *hex)
{
  int i;

  if (strlen(hex) != EXA_NODESET_HEX_SIZE)
    return -EXA_ERR_INVALID_VALUE;

  if (strspn(hex,  "0123456789abcdef") != EXA_NODESET_HEX_SIZE)
    return -EXA_ERR_INVALID_VALUE;

  for (i = EXA_NODESET_NB_CELLS - 1; i >= 0; i--)
  {
    sscanf(hex, EXA_NODESET_CELL_FORMAT, &set->cells[i]);
    hex += EXA_NODESET_BITS_PER_CELL / 4;
  }

  return EXA_SUCCESS;
}

/**
 * Create an hex string representing a node set.
 * The buffer should be EXA_NODESET_HEX_SIZE + 1.
 */
void exa_nodeset_to_hex(const exa_nodeset_t *set, char *hex)
{
  int i;
  for (i = EXA_NODESET_NB_CELLS - 1; i >= 0; i--)
    hex += sprintf(hex, EXA_NODESET_CELL_FORMAT, set->cells[i]);
}

/**
 * Create a binary string representing a node set.
 *
 * \param[in]  set  Node set to represent
 * \param[out] bin  Binary string
 *
 * String #bin should be at least EXA_MAX_NODES_NUMBER + 1 chars long
 * (including '\0').
 */
void exa_nodeset_to_bin(const exa_nodeset_t *set, char *bin)
{
  unsigned n;
  exa_nodeid_t node_id;
  int i = 0;

  n = exa_nodeset_num_bits(set);
  if (n > 0)
    {
      node_id = n - 1;
      while (true)
	{
	  bin[i++] = (char) (exa_nodeset_contains(set, node_id) ? '1' : '0');
	  if (node_id == 0)
	    break;

	  node_id--;
	}
    }

  bin[i] = '\0';
}
