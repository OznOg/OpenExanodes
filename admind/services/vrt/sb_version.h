/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/uuid.h"

typedef struct sb_version sb_version_t;

/**
 * Create a new sb_version with all metadata for a given group.
 * Sb_version remains loaded unpon return, which means that caller
 * MUST unload it or delete it before droping the pointer.
 *
 * @param[in]  uuid   uuid of the group we create a version for.
 *
 * @return a pointer on sb_version or NULL in case of error.
 */
sb_version_t *sb_version_new(const exa_uuid_t *uuid);

/**
 * Create a new invalid sb_version with all metadata for a given group.
 * Sb_version remains loaded upon return, which means that caller
 * MUST unload it or delete it before dropping the pointer.
 *
 * @param[in]  uuid   uuid of the group we create a version for.
 *
 * @return a pointer on sb_version or NULL in case of error.
 */
sb_version_t *sb_version_new_invalid(const exa_uuid_t *uuid);

/**
 * Delete a sb_version and all its metadata.
 *
 * @param[in]  sb_version  sb_version we want to delete.
 */
void sb_version_delete(sb_version_t *sb_version);

/**
 * Load a sb_version with all metadata for a given group.
 * The sb_version MUST have been created with sb_version_new
 * and unloaded.
 *
 * @param[in]  uuid   uuid of the group we want to load the sb_version.
 *
 * @return a pointer on sb_version or NULL in case of error.
 */
sb_version_t *sb_version_load(const exa_uuid_t *uuid);

/**
 * Unload a sb_version.
 * This flushes all metadata and releases memory.
 * After this call, the pointee passed in input is invalid.
 *
 * @param[in]  sb_version  sb_version we want to unload.
 */
void sb_version_unload(sb_version_t *sb_version);

/**
 * Ask sb_version to change its internal version.
 * After this call, version returned by sb_version_get_version()
 * will be different from the version before the call.
 * The version is saved and persistent, upon return any new read will return
 * the new version.
 * NOTE: this function is NOT thread safe.
 *
 * @param[in]  sb_version  sb_version we want to change the version.
 */
void sb_version_change_version(sb_version_t *sb_version);

/**
 * Retrieve the version of the sb_version.
 *
 * @param[in]  sb_version  sb_version we want to get the version.
 */
uint64_t sb_version_get_version(const sb_version_t *sb_version);

/**
 * Check if the version of the sb_version is valid.
 *
 * @param[in]  sb_version  sb_version we want to check.
 */
bool sb_version_is_valid(const sb_version_t *sb_version);

/**
 * Asks the local node to recover its sb_version.
 *
 * @param[in]  sb_version  sb_version we want to recover.
 */
void sb_version_local_recover(sb_version_t *sb_version);

/**
 * Asks the sb_version to prepare a new version (first phase).
 *
 * @param[in]  sb_version  sb_version we want to change.
 *
 * @return value of the sb_version if modification is eventually committed
 */
uint64_t sb_version_new_version_prepare(sb_version_t *sb_version);

/**
 * Notify the sb_version that 'prepare' step was done on all node.
 *
 * @param[in]  sb_version  sb_version we want to change.
 */
void sb_version_new_version_done(sb_version_t *sb_version);

/**
 * Notify the sb_version that 'done' step was done on all node.
 * Upon return, the new value of sb_version is committed.
 *
 * @param[in]  sb_version  sb_version we want to change.
 */
void sb_version_new_version_commit(sb_version_t *sb_version);

#define SB_VERSION_SERIALIZED_SIZE 32

typedef struct { char data[SB_VERSION_SERIALIZED_SIZE]; } sb_serialized_t;

/**
 * Serialize data of a sb_version
 *
 * @param[in]     sb_version  sb_version we want to serialize.
 * @param[in,out] buffer      sb_serialized_t buffer to get serialized data.
 */
void sb_version_serialize(const sb_version_t *sb_version,
                          sb_serialized_t *buffer);

/**
 * Update a sb_version from data contained in a sb_serialized_t buffer.
 *
 * @param[in] sb_version  sb_version we want to update.
 * @param[in] buffer      sb_serialized_t buffer of serialized data.
 */
void sb_version_update_from(sb_version_t *sb_version,
                            const sb_serialized_t *buffer);

/**
 * Delete the groups subdirectory in the cache dir.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int sb_version_remove_directory(void);
