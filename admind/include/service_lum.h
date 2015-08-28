/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __SERVICE_LUM_H
#define __SERVICE_LUM_H

/* FIXME This sould not appear here as it is private to the LUM service */
#include "admind/services/lum/include/service_lum_exports.h"

#include "admind/src/adm_nodeset.h"
#include "admind/services/lum/include/adm_export.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_mkstr.h"
#include "common/include/uuid.h"
#include "lum/export/include/export.h"
#include "target/iscsi/include/iqn.h"
#include "target/iscsi/include/lun.h"

#include "os/include/os_inttypes.h"

#define ADM_EXPORTS_FILE              "exports"
#define ADM_EXPORTS_FILE_VERSION      1
#define ADM_EXPORTS_FILE_VERSION_STR  exa_mkstr(ADM_EXPORTS_FILE_VERSION)

/**
 * @brief Get local target IQN
 *
 * @return pointer to the target IQN
 */
const iqn_t *lum_get_target_iqn(void);

/**
 * @brief Get a new LUN
 *
 * @param[out] lun      The LUN
 *
 * @return EXA_SUCCESS upon success. Otherwise an error code
 */
int lum_get_new_lun(lun_t *const lun);

/**
 * @brief Check if the given LUN is available or not
 *
 * @param[in] lun       The LUN
 *
 * @return true if available. Otherwise false.
 */
bool lum_lun_is_available(lun_t lun);

/**
 * @brief Load the exports XML file from disk
 *
 * @return EXA_SUCCESS if the operation succeeded, a negative error
 * code otherwise.
 */
exa_error_code lum_deserialize_exports(void);

/**
 * @brief Save the exports to XML file on disk
 *
 * @return EXA_SUCCESS if the operation succeeded, a negative error
 * code otherwise.
 */
exa_error_code lum_serialize_exports(void);

/**
 * @brief Delete the exports file
 *
 * @return true if the operation succeeded, false otherwise
 */
bool lum_exports_remove_exports_file(void);

/**
 * @brief Clears the whole table of exports.
 *
 */
void lum_exports_clear(void);

/**
 * @brief Increment the version number of the exports table.
 *
 */
void lum_exports_increment_version(void);

/**
 * @brief Removes an export from the exports table.
 *
 * @param[in] uuid	The uuid of the export to remove;
 */
void lum_exports_remove_export_from_uuid(const exa_uuid_t *uuid);

/**
 * Get info on all the export objects.
 *
 * The function allocates an array of structures and gives the
 * ownership to the calling function.
 *
 * @param[out] export_infos  Array of info
 *
 * @return the number of elements in the array or a negative error code
 */
int lum_exports_get_info(export_info_t **export_infos);

/**
 * @brief serialize the export_t of adm_export
 *
 * @param[in] uuid     uuid of the export
 *
 * @param[in:out] buf  buffer where to serialize
 *
 *  @param[in] size    size of the buffer, must be >= export_serialized_size()
 *
 *  return EXA_SUCCESS or a negative error code.
 */
int lum_exports_serialize_export_by_uuid(const exa_uuid_t *uuid, char *buf,
                                         size_t buf_size);

/**
 * @brief set an export to readonly or RW
 *
 * @param[in] uuid        uuid of the export
 *
 * @param[in] readonly
 */
void lum_exports_set_readonly_by_uuid(const exa_uuid_t *uuid, bool readonly);

/**
 * Ask the service lum to publish an export.
 *
 * @param[in] mh    examsg handle to be able to send a message
 *
 * @param[in] uuid  uuid of the volume to start
 *
 * return EXA_SUCCESS or a negative error code in case of error.
 */
int lum_service_publish_export(const exa_uuid_t *uuid);

/**
 * Master command to unpublish an export.
 *
 * @param[in] thr_nb   the thread id of the caller.
 * @param[in] uuid     uuid of the export to unpublish
 * @param[in] nodelist list of nodes on which export should be unpublished
 * @param[in] force    force unpublish even if errors occurs
 *
 * @return EXA_SUCCESS in case of success or a negative error code.
 */
int lum_master_export_unpublish(int thr_nb, const exa_uuid_t *uuid,
                                const exa_nodeset_t *nodelist, bool force);

/* FIXME Rename to lum_export_create_iscsi() and lum_export_create_bdev() */
int lum_create_export_iscsi(const exa_uuid_t *uuid, lun_t lun);
int lum_create_export_bdev(const exa_uuid_t *uuid, const char *path);

/******************************************************************************
 *                         FIXME: TEMPORARY GLUE CODE                         *
 ******************************************************************************/

/**
 * @brief Get the LUN for the export matching uuid.
 *
 * @param[in] uuid  the uuid of the export we're looking for
 *
 * @return the LUN if successful, LUN_NONE otherwise
 */
lun_t lum_exports_iscsi_get_lun_by_uuid(const exa_uuid_t *uuid);

/**
 * @brief Set the LUN for the export matching uuid.
 *
 * @param[in] uuid  the uuid of the export we're looking for
 * @param[in] lun  the new lun of the export we're looking for
 *
 * @return EXA_SUCCESS on success, a negative error on failure
 */
int lum_exports_iscsi_set_lun_by_uuid(const exa_uuid_t *uuid, lun_t lun);

/**
 * @brief Get the IQN filter policy for the export matching uuid.
 *
 * @param[in] uuid  the uuid of the export we're looking for
 *
 * @return the LUN or EXPORT_INVALID_PARAM
 */
iqn_filter_policy_t lum_exports_iscsi_get_filter_policy_by_uuid(const exa_uuid_t *uuid);

/**
 * @brief Set the IQN filter policy for the export matching uuid.
 *
 * @param[in] uuid  the uuid of the export we're looking for
 * @param[in] policy  the new filter policy of the export we're looking for
 *
 * @return EXA_SUCCESS on success, a negative error on failure
 */
int lum_exports_iscsi_set_filter_policy_by_uuid(const exa_uuid_t *uuid,
                                              iqn_filter_policy_t policy);

/**
 * Add an IQN filter to the export.
 *
 * @param[in] export           The export
 * @param[in] iqn_pattern_str  Filter's IQN pattern
 * @parma[in] policy           Whether IQNs matching the pattern are accepted
 *
 * @return EXA_SUCCESS if the add succeeded, EXA_ERR_INVALID_VALUE otherwise.
 */
exa_error_code lum_exports_iscsi_add_iqn_filter_by_uuid(const exa_uuid_t *uuid,
                                           const char *iqn_pattern_str,
                                           iqn_filter_policy_t policy);


/**
 * Remove an IQN filter from the export.
 *
 * @param[in] export           The export
 * @param[in] iqn_pattern_str  Pattern of the filter to remove
 *
 * @return EXA_SUCCESS if the removal succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code lum_exports_iscsi_remove_iqn_filter_by_uuid(const exa_uuid_t *uuid,
                                                           const char *iqn_pattern_str);

/**
 * Remove all IQN filters of a given policy.
 *
 * @param[in] export         The export
 * @param[in] policy         The policy to clear
 *
 * @return EXA_SUCCESS if the removal succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code lum_exports_iscsi_clear_iqn_filters_policy_by_uuid(const exa_uuid_t *uuid,
                                                            iqn_filter_policy_t policy);

/**
 * Get the number of IQN filters for an export
 * @param[in] export         The export
 *
 * @return a number greater or equal than 0 if the get succeeded,
 * EXPORT_INVALID_PARAM or EXPORT_INVALID_VALUE otherwise.
 */
int lum_exports_iscsi_get_iqn_filters_number_by_uuid(const exa_uuid_t *uuid);

/**
 * Get the Nth IQN filter for an export
 *
 * @param[in] export         The export
 * @param[in] n              The IQN filter number
 *
 * @return The pattern of the nth IQN filter if the get succeeded, NULL otherwise.
 */
const iqn_t *lum_exports_iscsi_get_nth_iqn_filter_by_uuid(const exa_uuid_t *uuid, int n);

/**
 * Get the Nth IQN filter for an export
 *
 * @param[in] export         The export
 * @param[in] n              The IQN filter number
 *
 * @return The policy of the nth IQN filter if the get succeeded, NULL
 * otherwise.
 */
iqn_filter_policy_t lum_exports_iscsi_get_nth_iqn_filter_policy_by_uuid(
                                            const exa_uuid_t *uuid, int n);

/**
 * Get the type of an export
 *
 * @param[in] export         The export
 *
 * @return The type of the export.
 */
export_type_t lum_exports_get_type_by_uuid(const exa_uuid_t *uuid);

/******************************************************************************
 *                         FIXME: END OF TEMPORARY GLUE CODE                  *
 ******************************************************************************/

#endif
