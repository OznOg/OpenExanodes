/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXPORT_H
#define __EXPORT_H

#include <time.h>

#include <sys/types.h>

#include "common/include/exa_error.h"
#include "common/include/uuid.h"
#include "common/include/exa_constants.h"
#include "target/iscsi/include/iqn.h"
#include "target/iscsi/include/iqn_filter.h"
#include "target/iscsi/include/iscsi.h"

/** An export */
typedef struct export export_t;

#define EXPORT_INVALID_VALUE 0xfffe
#define EXPORT_INVALID_PARAM 0xffff

typedef enum
{
    EXPORT_BDEV,
    EXPORT_ISCSI
} export_type_t;

#define EXPORT_TYPE__FIRST  EXPORT_BDEV
#define EXPORT_TYPE__LAST   EXPORT_ISCSI

#define EXPORT_TYPE__INVALID ((export_type_t)(EXPORT_TYPE__LAST + 1))

#define EXPORT_TYPE_IS_VALID(type) \
    ((type) >= EXPORT_TYPE__FIRST && (type) <= EXPORT_TYPE__LAST)

/**
 * Allocate an export.
 *
 * NOTE: The export is uninitialized.
 *
 * @return allocated export if successful, NULL otherwise.
 */
export_t *export_new(void);

/**
 * Allocate a BDEV export with the specified UUID and path
 *
 * @param[in] uuid           Export UUID
 * @param[in] path           Blockdevice's path
 *
 * @return export if successfully allocated, NULL otherwise
 */
export_t *export_new_bdev(const exa_uuid_t *uuid, const char *path);

/**
 * Allocate an ISCSI export with the specified UUID, lun, and IQN filter policy.
 *
 * @param[in] uuid           Export UUID
 * @param[in] lun            The target LUN
 * @param[in] filter_policy  The IQN filter policy
 *
 * @return export if successfully allocated, NULL otherwise
 */
export_t *export_new_iscsi(const exa_uuid_t *uuid, lun_t lun,
                           iqn_filter_policy_t filter_policy);

/**
 * Free an export
 * @param[in,out] export     The export to be freed
 */
void export_delete(export_t *export);

/**
 * Check whether an export is equal to another.
 *
 * @param[in] export1  Export
 * @param[in] export2  Export
 *
 * @return true if the exports are equal, false otherwise
 */
bool export_is_equal(const export_t *export1, const export_t *export2);

/**
 * Get the size of a serialized export.
 *
 * Use this function to determine the size of the buffer to be passed
 * to export_serialize().
 *
 * @return Size of serialized export, in bytes
 */
size_t export_serialized_size(void);

/**
 * Serialize an export.
 *
 * @param[in]  export  Export to serialize
 * @param[out] buf     Buffer to write to
 * @param[in]  size    Size of the buffer
 *
 * The buffer size must be at least export_serialized_size() bytes.
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise.
 */
int export_serialize(const export_t *export, void *buf, size_t size);

/**
 * Deserialize an export.
 *
 * @param[out] export  Deserialized export
 * @param[in]  buf     Buffer to read from
 * @param[in]  size    Size of the buffer
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int export_deserialize(export_t *export, const void *buf, size_t size);

/**
 * Return the export type.
 *
 * @param[in] export         The export
 *
 * @return the export type, or EXPORT_INVALID_PARAM if the export is NULL.
 */
export_type_t export_get_type(const export_t *export);

/**
 * Return the export uuid.
 *
 * @param[in] export         The export
 *
 * @return the export uuid, or NULL if the export is NULL.
 */
const exa_uuid_t *export_get_uuid(const export_t *export);

/**
 * Return is an export is readonly.
 *
 * @param[in] export         The export
 *
 * @return true if in readonly false if read/write
 */
bool export_is_readonly(const export_t *export);

/**
 * Set an export as read only or read write
 *
 * @param[in] export         The export
 *
 * @param[in] readonly       turn export to readonly if true RW else
 */
void export_set_readonly(export_t *export, bool readonly);

/**
 * Return the export path.
 *
 * @param[in] export         The export
 *
 * @return the export path, or NULL if the export is NULL or not a bdev
 * export.
 */
const char *export_bdev_get_path(const export_t *export);

/**
 * Return the export LUN.
 *
 * @param[in] export         The export
 *
 * @return the export LUN, or LUN_NONE if the export is invalid or not an
 *         iSCSI export.
 */
lun_t export_iscsi_get_lun(const export_t *export);

/**
 * Copy the IQN filters from an iSCSI export to another one.
 *
 * @param[in] export1  Export
 * @param[in] export2  Export
 */
void export_iscsi_copy_iqn_filters(export_t *dest, const export_t *src);

/**
 * Set the export LUN.
 *
 * @param[in] export         The export
 * @param[in] new_lun        The new LUN
 *
 * @return EXA_SUCCESS if the set succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code export_iscsi_set_lun(export_t *export, lun_t new_lun);

/**
 * Get the export IQN filter policy.
 *
 * @param[in] export         The export
 *
 * @return the IQN filter policy.
 */
iqn_filter_policy_t export_iscsi_get_filter_policy(const export_t *export);

/**
 * Change the export IQN filter policy.
 *
 * @param[in] export         The export
 * @param[in] new_policy     The new IQN filter policy
 *
 * @return EXA_SUCCESS if the set succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code export_iscsi_set_filter_policy(export_t *export,
                                              iqn_filter_policy_t new_policy);

/**
 * Add an IQN filter to the export.
 *
 * @param[in] export         The export
 * @param[in] iqn            The IQN pattern to add
 * @parma[in] policy         Whether this IQN is accepted
 *
 * @return EXA_SUCCESS if the add succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code export_iscsi_add_iqn_filter(export_t *export, const iqn_t *iqn,
                                           iqn_filter_policy_t policy);

/**
 * Get the export IQN policy for the given IQN.
 *
 * @param[in] export         The export
 * @param[in] iqn            The IQN
 *
 * @return the filter policy for the given IQN, or the global policy if
 * not found.
 */
iqn_filter_policy_t export_iscsi_get_policy_for_iqn(const export_t *export,
                                                    const iqn_t *iqn);

/**
 * Remove an IQN filter from the export.
 *
 * @param[in] export         The export
 * @param[in] iqn            The IQN pattern to remove
 *
 * @return EXA_SUCCESS if the removal succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code export_iscsi_remove_iqn_filter(export_t *export,
                                              const iqn_t *iqn_pattern);

/**
 * Remove all IQN filters.
 *
 * @param[in] export         The export
 *
 * @return EXA_SUCCESS if the removal succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code export_iscsi_clear_iqn_filters(export_t *export);

/**
 * Remove all IQN filters of a given policy.
 *
 * @param[in] export         The export
 * @param[in] policy         The policy to clear
 *
 * @return EXA_SUCCESS if the removal succeeded, EXPORT_INVALID_PARAM or
 * EXPORT_INVALID_VALUE otherwise.
 */
exa_error_code export_iscsi_clear_iqn_filters_policy(export_t *export,
                                                     iqn_filter_policy_t policy);

/**
 * Get the number of IQN filters for an export
 * @param[in] export         The export
 *
 * @return a number greater or equal than 0 if the get succeeded,
 * EXPORT_INVALID_PARAM or EXPORT_INVALID_VALUE otherwise.
 */
int export_iscsi_get_iqn_filters_number(const export_t *export);

/**
 * Get the nth IQN filter for an export.
 *
 * @param[in] export  Export to get the filter for
 * @param[in] n       Filter number
 *
 * @return The nth IQN filter if successful, NULL otherwise
 */
const iqn_filter_t *export_iscsi_get_nth_iqn_filter(const export_t *export,
                                                    int n);

/** Information on an export (meant for clinfo) */
typedef struct {
    exa_uuid_t uuid;
    export_type_t type;
    union
    {
	struct {
	    char path[EXA_MAXSIZE_DEVPATH + 1];
	} bdev;
        struct {
	    lun_t lun;
	} iscsi;
    };
} export_info_t;

/**
 * Get info on an export.
 *
 * @param[in]  export  Export to get info on
 * @param[out] info    Resulting info
 *
 * @return EXA_SUCCESS or a negative error code
 */
void export_get_info(const export_t * export, export_info_t *info);


#endif /* __EXPORT_H */
