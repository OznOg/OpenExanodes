/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include "vrt/virtualiseur/include/spof_group.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/common/include/spof.h"

#include "common/include/uuid.h"

#include "os/include/os_inttypes.h"

/**
 * Converts from number of sectors to number of kilobytes
 */
#define SECTORS_2_KBYTES(nb_sectors) ((nb_sectors)/2)
/**
 * Converts from number of kilobytes to number of sectors
 */
#define KBYTES_2_SECTORS(nb_kbytes) ((nb_kbytes)*2)

/** A storage entity */
typedef struct
{
    spof_group_t *spof_groups;  /**< Spof groups constituting the storage */
    uint32_t num_spof_groups;   /**< Number of spof groups */

    uint32_t chunk_size;        /**< Chunk size for all rdevs in the storage, in sectors */
} storage_t;

/**
 * Allocate and initialize a storage entity.
 *
 * @param[out] storage          Storage to initialize
 *
 * @return a pointer to the storage if successful, or NULL if an error happened.
 */
storage_t *storage_alloc(void);

/**
 * Free a storage entity.
 *
 * XXX At the moment, the free does *not* free the spof groups inside the
 *     storage entity.
 *
 * @param[in,out] storage  Storage to clean
 */
void __storage_free(storage_t *storage);
#define storage_free(storage) (__storage_free(storage), (storage) = NULL)

/**
 * Add (define) a spof group to a storage entity.
 *
 * @param[in,out] storage  Storage to add to
 * @param[in]     spof_id  Id of the spof group to add
 * @param[out]    sg       The added spof group
 *
 * @attention __only public for unit testing__. storage_add_rdev() adds the
 * spof group itself if needed. __Don't use manually.__
 *
 * @return 0 if successful, -EINVAL if a parameter is invalid, -EEXIST if the
 *         spof group is already defined, and -ENOSPC if all spof groups have
 *         been defined already
 */
int storage_add_spof_group(storage_t *storage, spof_id_t spof_id,
                                  spof_group_t **sg);

/**
 * Get a spof group by ID
 *
 * @param[in]   storage     The storage
 * @param[in]   spof_id     The spof_id
 *
 * @return the corresponding spof group
 */
spof_group_t *storage_get_spof_group_by_id(const storage_t *storage,
                                           spof_id_t spof_id);

/**
 * Get the total number of realdevs involved in the storage.
 *
 * @param[in]   storage Storage entity
 *
 * @return total number of realdevs in the storage.
 */
int storage_get_num_realdevs(const storage_t *storage);


/**
 * Get the total number of chunks in a spof group of a storage entity.
 *
 * @param[in] storage  Storage entity
 * @param[in] spof_id  Id of spof group to get the count of
 *
 * @return total number of chunks in the spof group
 */
uint64_t storage_get_spof_group_total_chunk_count(const storage_t *storage,
                                                  spof_id_t spof_id);

/**
 * Get the number of free chunks in a spof group of a storage entity.
 *
 * @param[in] storage  Storage entity
 * @param[in] spof_id  Id of spof group to get the count of
 *
 * @return number of free chunks in the spof group
 */
uint64_t storage_get_spof_group_free_chunk_count(const storage_t *storage,
                                                 spof_id_t spof_id);

/** Iterator over the spof groups of a storage entity */
typedef struct
{
    const storage_t *storage;  /**< Storage being iterated over */
    uint32_t index;            /**< Current position */
} storage_spof_iter_t;

/**
 * Initialize an iterator over a storage entity's spof groups.
 *
 * Must be called before storage_spof_iterator_get().
 *
 * @param[out] iter     Iterator
 * @param[in]  storage  Storage to iterate over
 */
void storage_spof_iterator_begin(storage_spof_iter_t *iter, const storage_t *storage);

/**
 * Cleanup an iteration over a storage entity's spof groups.
 *
 * Must be called once done with storage_spof_iterator_get().
 *
 * @param[in] iter  Iterator
 */
void storage_spof_iterator_end(storage_spof_iter_t *iter);

/**
 * Yield the id of a spof group in a storage entity.
 *
 * @param[in,out] iter  Iterator over a storage entity
 *
 * @return valid spof group id if the iteration is not over,
 *         and SPOF_ID_NONE otherwise
 */
spof_id_t storage_spof_iterator_get(storage_spof_iter_t *iter);

/** Iterator over the spof groups of a storage entity */
typedef struct
{
    const storage_t *storage;  /**< Storage being iterated over */
    uint32_t spof_group_index; /**< Current position in the storage's spof groups */
    uint32_t realdev_index;    /**< Current postion in the spof group's rdevs */
} storage_rdev_iter_t;

/**
 * Initialize an iterator over a storage entity's spof groups.
 *
 * Must be called before storage_spof_iterator_get().
 *
 * @param[out] iter     Iterator
 * @param[in]  storage  Storage to iterate over
 */
void storage_rdev_iterator_begin(storage_rdev_iter_t *iter, const storage_t *storage);

/**
 * Cleanup an iteration over a storage entity's realdevs.
 *
 * Must be called once done with storage_rdev_iterator_get().
 *
 * @param[in] iter  Iterator
 */
void storage_rdev_iterator_end(storage_rdev_iter_t *iter);

/**
 * Yield a realdev in a storage entity.
 *
 * @param[in,out] iter  Iterator over a storage entity
 *
 * @return valid vrt_realdev_t if the iteration is not over,
 *         and NULL otherwise
 */
vrt_realdev_t *storage_rdev_iterator_get(storage_rdev_iter_t *iter);

/**
 * Add an rdev to a storage entity.
 *
 * @param[in,out] storage  Storage to add to
 * @param[in]     spof_if  Id of the spof group containing the rdev
 * @param[in]     rdev     Rdev to add
 *
 * @return 0 if successful, -EINVAL if a parameter is invalid, -ENOENT
 *         if the specified spof group is not found, or another negative
 *         error code
 */
int storage_add_rdev(storage_t *storage, spof_id_t spof_id, vrt_realdev_t *rdev);

/**
 * Remove an rdev from a storage entity.
 *
 * @param[in,out] storage  Storage to remove from
 * @param[in]     spof_if  Id of the spof group containing the rdev
 * @param[in]     rdev     Rdev to remove
 *
 * @return 0 if successful, -EINVAL if a parameter is invalid, -ENOENT
 *         if the specified spof group is not found, or another negative
 *         error code
 */
int storage_del_rdev(storage_t *storage, spof_id_t spof_id, vrt_realdev_t *rdev);

/**
 * Get a specific rdev from a storage.
 *
 * @param[in] storage  Storage to get the rdev from
 * @param[in] uuid     UUID of the rdev to get
 *
 * @return rdev if found, NULL otherwise
 */
vrt_realdev_t *storage_get_rdev(const storage_t *storage, const exa_uuid_t *uuid);

/**
 * Cut the rdevs in the storage into chunks of storage's chunk size.
 *
 * @param[in]   storage The storage
 * @param[in]   chunk_size   Size of chunks, in bytes
 *
 * @return 0 if sucessful, a negative error code otherwise.
 */
int storage_cut_in_chunks(storage_t *storage, uint32_t chunk_size);

/**
 * Initialize an rdev chunks counts.
 *
 * @param[in]   storage             The storage
 * @param[in]   storage             The rdev
 * @param[in]   total_chunks_count  The total number of chunks
 */
void storage_initialize_rdev_chunks_info(storage_t *storage, vrt_realdev_t *rdev,
                                        uint64_t total_chunks_count);

int storage_cut_rdev_in_chunks(storage_t *storage, vrt_realdev_t *rdev);

typedef enum { STORAGE_HEADER_MAGIC = 0x7700FFCC } storage_header_magic_t;

#define STORAGE_HEADER_FORMAT  1

typedef struct
{
    /* IMPORTANT - Fields 'magic' and 'format' *MUST* always be 1st and 2nd */
    storage_header_magic_t magic;
    uint32_t format;
    uint32_t chunk_size;
    uint32_t nb_rdevs;
} storage_header_t;

int storage_header_read(storage_header_t *header, stream_t *stream);

uint64_t storage_serialized_size(const storage_t *storage);
int storage_serialize(const storage_t *storage, stream_t *stream);
int storage_deserialize(storage_t *storage, stream_t *stream);

bool storage_equals(const storage_t *s1, const storage_t *s2);

typedef struct
{
    exa_uuid_t uuid;
    uint64_t total_chunks_count;
} storage_rdev_header_t;

int storage_rdev_header_read(storage_rdev_header_t *header, stream_t *stream);

#endif /* STORAGE_H */
