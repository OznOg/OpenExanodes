/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef ASSEMBLY_VOLUME_H
#define ASSEMBLY_VOLUME_H

#include "vrt/assembly/src/assembly_slot.h"

#include "vrt/common/include/vrt_stream.h"

#include "common/include/uuid.h"

#include "os/include/os_inttypes.h"

/* FIXME Should contain slot_width */
/**
 * Assembly of a volume that contains the indexes of its slots
 */
typedef struct assembly_volume assembly_volume_t;
struct assembly_volume
{
    exa_uuid_t uuid;            /**< UUID of the assembly volume */
    slot_t **slots;             /**< Slots used by the volume */
    uint64_t total_slots_count; /**< Number of slots used by the volume */

    assembly_volume_t *next;    /**< Next assembly volume in assembly group */
};

assembly_volume_t *assembly_volume_alloc(const exa_uuid_t *uuid);

void __assembly_volume_free(assembly_volume_t *av);
#define assembly_volume_free(av)  (__assembly_volume_free(av), (av) = NULL)

int assembly_volume_resize(assembly_volume_t *av, const storage_t *storage,
                           uint32_t slot_width, uint64_t new_slots_count);

/**
 * Tell whether an assembly volume is equal to another.
 *
 * @param[in] a  Assembly volume
 * @param[in] b  Assembly volume
 *
 * @return true if a and b are equal, false otherwise
 */
bool assembly_volume_equals(const assembly_volume_t *a, const assembly_volume_t *b);

/**
 * Compute the mapping of a sector from a volume to a slot.
 *
 * This function takes the slot size as a parameter, so that one can
 * compute either a logical mapping (when passing a logical slot size)
 * or a physical mapping (when passing a physical slot size).
 *
 * FIXME: we return the slot correspondance with an index which make this
 *        function incoherent with 'assembly_group_map_sector_to_slot'.
 *        By now, I don't see a better solution since this function is used to
 *        manage the slot metadata and we cannot associate the metadata block
 *        directly to the slot.
 *
 * @param[in]  ag                  Assembly group
 * @param[in]  slot_size           Slot size (either logical or physical)
 * @param[in]  gsector             Offset of the sector on the group
 * @param[out] slot_index          Index in the assembly volume of the slot
 *                                 that contains the sector.
 * @param[out] offset_in_slot      Offset of the sector in the slot
 */
void assembly_volume_map_sector_to_slot(const assembly_volume_t *av,
                                        uint64_t slot_size, uint64_t vsector,
                                        unsigned int *slot_index,
                                        uint64_t *offset_in_slot);

typedef enum { AV_HEADER_MAGIC = 0x77A44A22 } ag_volume_header_t;

typedef struct
{
    ag_volume_header_t magic;
    uint32_t reserved;
    exa_uuid_t uuid;
    uint64_t total_slot_count;
} av_header_t;

int assembly_volume_header_read(av_header_t *header, stream_t *stream);

uint64_t assembly_volume_serialized_size(const assembly_volume_t *av);

/**
 * Serialize an assembly volume onto a stream.
 *
 * @param[in] av      Assembly volume to serialize
 * @param     stream  Stream to write to
 *
 * @return 0 if successful, a negative error code otherwise
 */
int assembly_volume_serialize(const assembly_volume_t *av, stream_t *stream);

/**
 * Deserialize an assembly volume from a stream.
 *
 * @param[out] av      Deserialized assembly volume
 * @param[in]  storage The storage on which the volume is built.
 * @param      stream  Stream to read from
 *
 * @return 0 if successful, a negative error code otherwise
 */
int assembly_volume_deserialize(assembly_volume_t **av,
                                   const storage_t *storage, stream_t *stream);

#endif /* ASSEMBLY_VOLUME_H */
