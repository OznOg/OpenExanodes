/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ASSEMBLY_SLOT_H__
#define __ASSEMBLY_SLOT_H__

#include "vrt/virtualiseur/include/chunk.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/spof_group.h"
#include "vrt/virtualiseur/include/storage.h"

/**
 * Slot representation based on chunks
 */
typedef struct slot
{
    chunk_t **chunks; /**< Array of pointer on chunks that constitute the slot */
    uint32_t width;   /**< Number of elements in chunk array */
    void *private;    /**< Private data of the slot's user; This must be not
                           persistent data as it is obviously not serialized.
                           Deserialization sets it to NULL. */
} slot_t;

/**
 * Compute the mapping of a sector from a slot to a disk.
 *
 * @param[in]  slot         Slot that contains the sector
 * @param[in]  chunk_index  Index of the chunk that contains the sector
 *                          (between 0 and slot->width - 1)
 * @param[in]  offset       Offset of the sector in the slot
 *                          (between 0 and slot->height - 1)
 * @param[out] rdev         Computed real device that contains the sector
 * @param[out] rsector      Offset of the sector on the rdev
 */
void assembly_slot_map_sector_to_rdev(const slot_t *slot, unsigned int chunk_index,
                                      uint64_t offset, struct vrt_realdev **rdev,
                                      uint64_t *rsector);

struct slot *slot_make(spof_group_t *spof_groups,
                       uint32_t nb_spof_groups, uint32_t slot_width);

void __slot_free(struct slot *slot);
#define slot_free(s) ( __slot_free(s), (s) = NULL )

/**
 * Tell whether a slot is equal to another.
 *
 * @param[in] a  Slot
 * @param[in] b  Slot
 *
 * @return true if a and b are equal, false otherwise
 */
bool slot_equals(const slot_t *a, const slot_t *b);

typedef struct
{
    exa_uuid_t rdev_uuid;
    uint64_t offset;
} chunk_header_t;

int chunk_header_read(chunk_header_t *header, stream_t *stream);

typedef struct
{
    uint32_t width;
} slot_header_t;

int slot_header_read(slot_header_t *header, stream_t *stream);

uint64_t slot_serialized_size(const slot_t *slot);

/**
 * Serialize a slot onto a stream.
 *
 * @param[in] slot    Slot to serialize
 * @param     stream  Stream to write to
 *
 * @return 0 if successful, a negative error code otherwise
 */
int slot_serialize(const slot_t *slot, stream_t *stream);

/**
 * Deserialize a slot from a stream.
 *
 * @param[out] slot     Deserialized slot
 * @param[in]  storage  Storage to be looked up for rdevs comprising the slot
 * @param      stream   Stream to read from
 *
 * @return 0 if successful, a negative error code otherwise
 */
int slot_deserialize(slot_t **slot, const storage_t *storage, stream_t *stream);

/**
 * Dump a slot in user-friendly form to a stream.
 *
 * @param[in] slot    Slot
 * @param     stream  Stream to dump to
 *
 * @return 0 if successful, a negative error code otherwise
 */
int slot_dump(const slot_t *slot, stream_t *stream);

#endif /* __ASSEMBLY_SLOT_H__ */
