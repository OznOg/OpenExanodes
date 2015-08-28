/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXECUTIVE_EXPORT_H
#define __EXECUTIVE_EXPORT_H

#include "blockdevice/include/blockdevice.h"

#include "lum/export/include/export.h"
#include "lum/export/include/export_info.h"

#include "os/include/os_inttypes.h"
#include "target/iscsi/include/iqn.h"
#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

typedef struct lum_export lum_export_t;

typedef enum
{
    EXPORT_NOT_IN_USE,
    EXPORT_IN_USE,
    EXPORT_UNKNOWN_IN_USE
} lum_export_inuse_t;

/**
 * Set the readhead of an export.
 *
 * @param[in] export_uuid  UUID of the export
 * @param[in] readahead    New readahead value
 *
 * @return EXA_SUCCESS or a negative error code
 */
int lum_export_set_readahead(const exa_uuid_t *export_uuid, uint32_t readahead);


/**
 * Get the IQN of the Nth initiator connected to the target that can use the
 * export.
 *
 * The Nth initiator is not the initiator at index 'n' in the session array or
 * whatever the structure we use in the underlying components.  It is the Nth
 * initiator that can effectively use the export (if the function returns a
 * valid IQN for n, we are sure it would have returned a valid IQN for n-1).
 *
 * @param[in]  export_uuid  UUID of the export
 * @param[in]  n            number of the initiator
 * @param[ont] iqn          initiator's IQN
 *
 * @return EXA_SUCCESS or a negative error code
 */
int lum_export_get_nth_connected_iqn(const exa_uuid_t *export_uuid,
                                 unsigned int n,
                                 iqn_t *iqn);

/**
 * Get the UUID of an export.
 *
 * @param[in] export  Export to get the UUID of
 *
 * @return UUID if successful, NULL otherwise
 */
const exa_uuid_t *lum_export_get_uuid(const lum_export_t *export);

/*
 * callback when an io on an export is finished.
 * err contains the error code on the io, private_data is the private_data
 * given by export_submit_io caller */
typedef void lum_export_end_io_t(int err, void *private_data);

void lum_export_submit_io(lum_export_t *export, blockdevice_io_type_t op, bool flush_cache,
                          long long sector, int size, void *page,
                          void *private_data, lum_export_end_io_t *callback);

/**
 * Export an export.
 *
 * @param[in] buf   Buffer containing a serialized export
 * @param[in] size  Size of buffer
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int lum_export_export(const char *buf, size_t buf_size);
/* FIXME ^ Input should be an already deserialized export */

/**
 * Unexport an export.
 *
 * @param[in] export_uuid  UUID of export to unexport
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int lum_export_unexport(const exa_uuid_t *export_uuid);

/**
 * Update the IQN filters of an export.
 *
 * @param[in] buf   Buffer containing a serialized export
 * @param[in] size  Size of buffer
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int lum_export_update_iqn_filters(const char *buf, size_t buf_size);

/**
 * Resize (the volume underlying) an export.
 *
 * @param[in] uuid       UUID of export to resize
 * @param[in] newsizeKB  New size, in kibibytes
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int lum_export_resize(const exa_uuid_t *uuid, uint64_t newsizeKB);

/**
 * Tell whether an export is readonly.
 *
 * @param[in] export  Export
 *
 * @return true if readonly, false otherwise
 */
bool lum_export_is_readonly(const lum_export_t *export);

/**
 * Get information on an export, for clinfo.
 *
 * @param[in]  export_uuid  UUID of export to get clinfo on
 * @param[out] info         Resulting export info
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int lum_export_get_info(const exa_uuid_t *export_uuid, lum_export_info_reply_t *info);

/**
 * Get the description of a LUM export.
 *
 * @param[in] export  LUM export, *cannot* be NULL
 *
 * @return Export description
 */
export_t *lum_export_get_desc(const lum_export_t *export);

/**
 * Get the blockdevice of the volume underlying an export.
 *
 * @param[in] export  Export to get the volume blockdevice of, *cannot* be NULL
 *
 * @return Volume blockdevice
 */
blockdevice_t *lum_export_get_blockdevice(lum_export_t *export);

/**
 * Initialization of static data necessary to handle exports.
 * *Must* be called prior to using any function of this API.
 *
 * @param[in] node_id  Id of the local node
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int lum_export_static_init(exa_nodeid_t node_id);

/**
 * Clean up the static data used for handling exports.
 * *Must* be called once done with exports.
 */
void lum_export_static_cleanup(void);

#endif /* __EXECUTIVE_EXPORT_H */
