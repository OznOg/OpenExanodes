/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LUM_CLIENT_H
#define __LUM_CLIENT_H

/** \file lum_client.h
 *
 * Client-side interface to the LUM executive.
 */
#include "lum/export/include/export_info.h"

#include "target/iscsi/include/iqn.h"
#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "examsg/include/examsg.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_network.h"
#include "target/include/target_adapter.h"

/**
 * @brief Initialize the LUM executive.
 *
 * @param     mh      Examsg handle to use
 * @param[in] param   Initialization parameters
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_init(ExamsgHandle mh, const lum_init_params_t *params);

/**
 * Cleanup the LUM executive.
 *
 * @param mh  Examsg handle to use
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_cleanup(ExamsgHandle mh);

/**
 * @brief Suspend the LUM executive.
 *
 * @param  mh   Examsg handle to use
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_suspend(ExamsgHandle mh);

/**
 * @brief Resume the LUM executive.
 *
 * @param  mh   Examsg handle to use
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_resume(ExamsgHandle mh);

/**
 * Set the list of peers in lum. This sends Ip address and nodes id.
 *
 * @param     mh     Examsg handle to use
 * @param[in] peers  Structures which contains the list of peers.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_set_peers(ExamsgHandle mh, const adapter_peers_t *peers);

/**
 * Set the list of targets listening addresses in lum.
 *
 * @param     mh                Examsg handle to use
 * @param[in] num_addr          Number of addresses.
 * @param[in] target_addresses  The list of addresses.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_set_targets(ExamsgHandle mh, uint32_t num_addr,
                           const in_addr_t target_addresses[]);

/**
 * Set the membership the LUM executive has to work with.
 *
 * @param     mh     Examsg handle to use
 * @param[in] mship  New membership
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_set_membership(ExamsgHandle mh, const exa_nodeset_t *mship);

/**
 * @brief publish an export.
 * This function takes a serialized export_t given by export_serialize()
 * as argument.
 *
 * @param     mh         Examsg handle to use
 * @param[in] buf        serialized export buffer
 * @param[in] buf_size   size of the buffer
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_export_publish(ExamsgHandle mh, const char *buf, size_t buf_size);

/**
 * @brief unpublish an export.
 *
 * @param     mh      Examsg handle to use
 * @param[in] uuid    uuid of the export
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_export_unpublish(ExamsgHandle mh, const exa_uuid_t *export_uuid);

/**
 * Retrieve informations about an export.
 *
 * @param      mh           Examsg handle
 * @param[in]  export_uuid  UUID of export
 * @param[out] info         Informations about export
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_export_info(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                           lum_export_info_reply_t *info);

/**
 * Set the readahead of an export.
 *
 * @param     mh           Examsg handle to use
 * @param[in] export_uuid  UUID of export
 * @param[in] readahead    New value of readahead
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_set_readahead(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                             uint32_t readahead);

/**
 * Resize an export.
 *
 * @param     mh           Examsg handle to use
 * @param[in] export_uuid  UUID of export
 * @param[in] size    New size of export
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_export_resize(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                          uint64_t size);

/**
 * @brief Update the IQN filters.
 * This function takes a serialized export_t given by export_serialize()
 * as argument.
 *
 * @param     mh         Examsg handle to use
 * @param[in] buf        serialized export buffer
 * @param[in] buf_size   size of the buffer
 *
 * @return 0 if successful, a negative error code otherwise
 */
int lum_client_export_update_iqn_filters(ExamsgHandle mh, const char *buf, size_t buf_size);

int lum_client_get_nth_connected_iqn(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                                     unsigned int iqn_num, iqn_t *iqn);

/**
 * @brief Start the iSCSI target. To be done only when everything is ready.
 *
 * @param  mh   Examsg handle to use
 *
 */
int lum_client_start_target(ExamsgHandle mh);

/**
 * @brief Stop the iSCSI target.
 *
 * @param  mh   Examsg handle to use
 *
 */
int lum_client_stop_target(ExamsgHandle mh);
#endif  /* __LUM_CLIENT_H */

