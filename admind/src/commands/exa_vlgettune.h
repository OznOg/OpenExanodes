/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_VLGETTUNE_H
#define __EXA_VLGETTUNE_H

#include "admind/src/adm_volume.h"
#include "admind/src/commands/tunelist.h"

#define VLTUNE_PARAM_LUN                "lun"
#define VLTUNE_PARAM_IQN_AUTH_ACCEPT    "iqn_auth_accept"
#define VLTUNE_PARAM_IQN_AUTH_REJECT    "iqn_auth_reject"
#define VLTUNE_PARAM_IQN_AUTH_MODE      "iqn_auth_mode"
#define VLTUNE_PARAM_READAHEAD          "readahead"

/**
 * @brief Add LUN information to the tuning list
 */
int tunelist_add_lun(tunelist_t *tunelist, const struct adm_volume *volume);

/**
 * @brief Add the list of allowed IQN to a tuning list
 *
 * @param[out] tunelist  tuning list in construction
 * @param[in]  volume    pointer to the volume
 *
 * @return EXA_SUCCESS or an error code in case of failure
 */
int tunelist_add_iqn_auth_accept(tunelist_t *tunelist, const struct adm_volume *volume);

/**
 * @brief Add the list of not allowed IQN to a tuning list
 *
 * @param[out] tunelist  tuning list in construction
 * @param[in]  volume    pointer to the volume
 *
 * @return EXA_SUCCESS or an error code in case of failure
 */
int tunelist_add_iqn_auth_reject(tunelist_t *tunelist, const struct adm_volume *volume);

/**
 * @brief Add IQN authorization mode to a tuning list
 *
 * @param[out] tunelist  tuning list in construction
 * @param[in]  volume    pointer to the volume
 *
 * @return EXA_SUCCESS or an error code in case of failure
 */
int tunelist_add_iqn_auth_mode(tunelist_t *tunelist, const struct adm_volume *volume);

/**
 * @brief Add readahead information to the tuning list
 */
int tunelist_add_readahead(tunelist_t *tunelist, const struct adm_volume *volume);

#endif
