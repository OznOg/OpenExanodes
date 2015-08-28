/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SPOF_H__
#define __SPOF_H__

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "os/include/os_inttypes.h"

/** Maximum number of spofs */
#define SPOFS_MAX  EXA_MAX_NODES_NUMBER

/** Numerical id for a SPOF. */
typedef uint32_t spof_id_t;

/** Special SPOF id for 'unset spof' */
#define SPOF_ID_NONE 0

/** Check whether a SPOF id is valid */
#define SPOF_ID_IS_VALID(id) ((id) > 0)

/** Format for printing SPOF ids */
#define PRIspof_id  PRIu32

/**
 * Convert a string to a SPOF id.
 *
 * @param[out] spof_id  SPOF id parsed
 * @param[in]  str      String to convert
 *
 * @return EXA_SUCCESS if successful, -EINVAL otherwise
 */
int spof_id_from_str(spof_id_t *spof_id, const char *str);

/**
 * Convert a SPOF id to a string.
 *
 * @param[in] spof_id  SPOF id to convert
 *
 * @return SPOF id string if successful, NULL otherwise
 */
const char *spof_id_to_str(spof_id_t spof_id);

/** Structure describing a SPOF */
typedef struct
{
    spof_id_t id;         /**< Id of the SPOF */
    exa_nodeset_t nodes;  /**< Nodes participating in the SPOF */
} spof_t;

/**
 * Initialize a SPOF.
 * @param[in] spof  The SPOF to initialise
 */
void spof_init(spof_t *spof);

/**
 * Set the spof ID in a spof SPOF.
 * @param[in] spof  The SPOF
 * @param[in] id    The ID to set
 *
 * @return EXA_SUCCESS if successful, or a negative error code.
 */
int spof_set_id(spof_t *spof, spof_id_t id);

/**
 * Get a given SPOF's ID.
 * @param[in] spof      The SPOF to look at
 *
 * @return the spof ID.
 */
spof_id_t spof_get_id(const spof_t *spof);

/**
 * Get the number of nodes contained in a given SPOF.
 * @param[in] spof  The SPOF to look at
 *
 * @return the number of nodes in the SPOF.
 */
unsigned int spof_get_num_nodes(const spof_t *spof);

/**
 * Get all the nodes in a SPOF.
 *
 * @param[in] spof  The SPOF to get the nodes of
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise.
 */
int spof_get_nodes(const spof_t *spof, exa_nodeset_t *nodes);

/**
 * Add a node id in a given SPOF.
 * @param[in] spof      The SPOF to insert the node id in
 * @param[in] node      The node id.
 */
void spof_add_node(spof_t *spof, const exa_nodeid_t node);

/**
 * Remove a node id from a given SPOF.
 * @param[in] spof      The SPOF to remove the node id from
 * @param[in] node      The node id.
 */
void spof_remove_node(spof_t *spof, const exa_nodeid_t node);

/**
 * Returns whether a node id is contained in a SPOF
 * @param[in] spof      The SPOF to look at
 * @param[in] node      The node id.
 *
 * @return true if node is in spof, false otherwise.
 */
bool spof_contains_node(const spof_t *spof, const exa_nodeid_t node);

/**
 * Copies a spof to another.
 * @param[in,out] dest      The destination SPOF
 * @param[in] src           The source SPOF
 */
void spof_copy(spof_t *dest, const spof_t *src);

/**
 * Lookup a spof by id in an array of spofs.
 *
 * @param[in] spofs      Array of spofs
 * @param[in] num_spofs  Number of spofs in the array
 * @param[in] id         Id of spof to lookup
 *
 * @return Spof if found, NULL otherwise
 */
const spof_t *spof_lookup(const spof_t *spofs, unsigned num_spofs, spof_id_t id);

#endif /* __SPOF_H__ */
