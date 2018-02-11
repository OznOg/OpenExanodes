/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_NODESET_H__
#define __EXA_NODESET_H__

#include "common/include/exa_constants.h"
#include "os/include/os_assert.h"
#include "os/include/os_inttypes.h"

/* Bit field for node sets has to be larger than the host bus size.
 * So we use a table of EXA_NODESET_BITS_PER_CELL unsigned ints.
 * Then the number of cells needed to hold EXA_MAX_NODES_NUMBER bits is
 * computed as floor (EXA_MAX_NODES_NUMBER / EXA_NODESET_BITS_PER_CELL),
 * and floor(x / y) = ((x - 1) / y) + 1
 */

/** Number of bits in a bitfield cell. We choose 64 bits so we do not need
 *  padding in examsg. */
#define EXA_NODESET_CELL_TYPE uint64_t

#define EXA_NODESET_BITS_PER_CELL (sizeof(EXA_NODESET_CELL_TYPE) * 8)

/** Number of cells in a bitfield */
#define EXA_NODESET_NB_CELLS (((EXA_MAX_NODES_NUMBER - 1) / EXA_NODESET_BITS_PER_CELL) + 1)

/** Numerical id for a node. We choose an int to simplify padding in examsg */
typedef unsigned int exa_nodeid_t;

/** Special node id for localhost */
#define EXA_NODEID_LOCALHOST  (EXA_MAX_NODES_NUMBER + 1)

/** Special node id for 'no node' */
#define EXA_NODEID_NONE  (EXA_MAX_NODES_NUMBER + 5)

/** Check whether a node id is valid */
#define EXA_NODEID_VALID(id) \
    ((int)(id) >= 0 && (int)(id) < EXA_MAX_NODES_NUMBER)

/** Format for printing node ids */
#define PRInodeid  "u"

/**
 * Convert a string to a node id.
 *
 * @param[out] node_id  Node id parsed
 * @param[in]  str      String to convert
 *
 * @return EXA_SUCCESS if successful, -EINVAL otherwise
 */
int exa_nodeid_from_str(exa_nodeid_t *node_id, const char *str);

/**
 * Convert a node id to a string.
 *
 * @param[in] node_id  Node id to convert
 *
 * @return Node id string if successful, NULL otherwise
 */
const char *exa_nodeid_to_str(exa_nodeid_t node_id);

/** bitfield to store a node set */
typedef struct {
  EXA_NODESET_CELL_TYPE cells[EXA_NODESET_NB_CELLS];
} __attribute__((packed, aligned(sizeof(EXA_NODESET_CELL_TYPE)))) exa_nodeset_t;


/** Iterator on a node set */
typedef exa_nodeset_t exa_nodeset_iter_t;

void exa_nodeset_reset(exa_nodeset_t *set);
void exa_nodeset_copy(exa_nodeset_t *to, const exa_nodeset_t *from);

bool exa_nodeset_is_empty(const exa_nodeset_t *set);

void exa_nodeset_add(exa_nodeset_t *set, exa_nodeid_t node);
void exa_nodeset_del(exa_nodeset_t *set, exa_nodeid_t node);

bool exa_nodeset_contains(const exa_nodeset_t *set, exa_nodeid_t node);
bool exa_nodeset_equals(const exa_nodeset_t *set1, const exa_nodeset_t *set2);


void exa_nodeset_sum(exa_nodeset_t *to, const exa_nodeset_t *from);
void exa_nodeset_substract(exa_nodeset_t *to, const exa_nodeset_t *from);
void exa_nodeset_intersect(exa_nodeset_t *to, const exa_nodeset_t *from);

bool exa_nodeset_included(const exa_nodeset_t *a, const exa_nodeset_t *b);
bool exa_nodeset_disjoint(const exa_nodeset_t *a, const exa_nodeset_t *b);

int  exa_nodeset_count(const exa_nodeset_t *set);
unsigned exa_nodeset_num_bits(const exa_nodeset_t *set);

exa_nodeid_t exa_nodeset_first_at(const exa_nodeset_t *set, exa_nodeid_t node);
exa_nodeid_t exa_nodeset_first(const exa_nodeset_t *set);

void exa_nodeset_iter_init(const exa_nodeset_t *set, exa_nodeset_iter_t *iter);
int  exa_nodeset_iter(exa_nodeset_iter_t *iter, exa_nodeid_t *node);

int exa_nodeset_from_hex(exa_nodeset_t *set, const char *hex);

void exa_nodeset_to_hex(const exa_nodeset_t *set, char *hex);
void exa_nodeset_to_bin(const exa_nodeset_t *set, char *bin);

/* Build a set containing a single specified node.
   If the node is localhost, builds an empty set */
#define exa_nodeset_single(set, node)		\
  do {						\
    exa_nodeset_reset(set);			\
    if ((node) != EXA_NODEID_LOCALHOST)		\
      exa_nodeset_add((set), (node));		\
  } while (0)

/**
 * A simple iterator. You can use it like a for loop.
 *
 * Usage:
 *   exa_nodeid_t node;
 *   exa_nodeset_foreach(set, node)
 *   {
 *     foo(node);
 *   }
 */
#define exa_nodeset_foreach(set, node)           \
  for(node = exa_nodeset_first_at(set, 0);       \
      node < EXA_MAX_NODES_NUMBER;               \
      node = (node + 1 < EXA_MAX_NODES_NUMBER) ? \
       exa_nodeset_first_at(set, node + 1): node + 1 \
      )


static inline __attribute__((unused)) void __check_config_consistency(void)
{
    /* If something doesn't compile here, please fix EXA_NODESET_* below to
     * match EXA_NODESET_NB_CELLS */
    COMPILE_TIME_ASSERT(EXA_NODESET_NB_CELLS == 2);
    COMPILE_TIME_ASSERT(sizeof(EXA_NODESET_CELL_TYPE) == 8);
}

#define EXA_NODESET_CELL_FORMAT "%016" PRIx64
#define EXA_NODESET_FMT EXA_NODESET_CELL_FORMAT EXA_NODESET_CELL_FORMAT
#define EXA_NODESET_VAL(set) (set)->cells[1], (set)->cells[0]
extern const exa_nodeset_t EXA_NODESET_EMPTY;
extern const exa_nodeset_t EXA_NODESET_FULL;
#define EXA_NODESET_HEX_SIZE (EXA_NODESET_NB_CELLS * 16)
#define EXA_NODESET_PROP_EMPTY "00000000000000000000000000000000"

#endif /* __EXA_NODESET_H__ */
