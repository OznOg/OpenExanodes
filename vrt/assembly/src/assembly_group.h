/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ASSEMBLY_GROUP_H__
#define __ASSEMBLY_GROUP_H__

#include "vrt/assembly/src/assembly_volume.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/chunk.h"

#include "vrt/common/include/spof.h"
#include "vrt/common/include/vrt_stream.h"

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

#include "os/include/os_inttypes.h"

/**
 * Assembly of a group. It contains:
 * - the array of the disks that are part of the assembly
 * - the list of SPOF groups
 * - the array of slots
 */
typedef struct assembly_group
{
    /* FIXME That's crap, get rid of it */
    bool initialized;

    /* Information about the slots */
    uint32_t slot_size;             /**< Physical slot size (in sectors) */
    uint32_t slot_width;            /**< Slot width (number of chunks per slot) */
    assembly_volume_t *subspaces;   /**< Subspaces of the assembly group */
    uint32_t num_subspaces;         /**< Number of subspaces */
} assembly_group_t;

/**
 * Setup an assembly group (but not its slots!).
 *
 * @param[in,out] ag          Assembly group that owns the assembly
 * @param[in]     slot_width  Slot width (number of chunks per slot)
 * @param[in]     chunk_size  Size of a chunk (in sectors)
 */
int assembly_group_setup(assembly_group_t *ag, uint32_t slot_width,
                         uint32_t chunk_size);

/**
 * Initialize an assembly group with sane values.
 *
 * @param[in,out] ag          Assembly group to init
 */
void assembly_group_init(assembly_group_t *ag);

/**
 * @brief return the maximum number of slots for an assembly group
 *
 * The maximum number of slots that a group may contain is deduced
 * from the total number of chunks and the requested slot width.
 *
 * @param[in] ag         Assembly group
 * @param[in] storage    The storage
 *
 * @return Maximum number of slots for the group
 */
uint64_t assembly_group_get_max_slots_count(const assembly_group_t *ag,
                                            const storage_t *storage);

/**
 * @brief return the number of slots that are currently used by an assembly group
 *
 * @param[in] ag  Assembly group
 *
 * @return number of slots used in the assembly group
 */
uint64_t assembly_group_get_used_slots_count(const assembly_group_t *ag);

/**
 * Cleanup an assembly group.
 *
 * The slots and subspaces comprising the assembly group are removed and
 * cleaned up (freed).
 *
 * @param[in,out] ag  Assembly group to clean up
 */
void assembly_group_cleanup(assembly_group_t *ag);

/**
 * Tell whether an assembly group is equal to another.
 *
 * @param[in] a  Assembly group
 * @param[in] b  Assembly group
 *
 * @return true if a and b are equal, false otherwise
 */
bool assembly_group_equals(const assembly_group_t *a, const assembly_group_t *b);

/**
 * Compute the mapping of a sector from a group to a slot.
 *
 * This function takes the slot size as a parameter, so that one can
 * compute either a logical mapping (when passing a logical slot size)
 * or a physical mapping (when passing a physical slot size).
 *
 * @param[in]  ag         Assembly group
 * @param[in]  slot_size  Slot size (either logical or physical)
 * @param[in]  gsector    Offset of the sector on the group
 * @param[out] slot       Slot that contains the sector
 * @param[out] offset     Offset of the sector in the slot
 */
void assembly_group_map_sector_to_slot(const assembly_group_t *ag,
                                       const assembly_volume_t *av,
                                       uint64_t slot_size,
                                       uint64_t vsector,
                                       const slot_t **slot,
                                       uint64_t *offset_in_slot);

/**
 * Get the slot width.
 *
 * @param[in]  ag   Assembly group
 *
 * @return the number of chunks in each slot
 */
uint32_t assembly_group_get_slot_width(const assembly_group_t *ag);

/**
 * Get the number of available slots.
 *
 * @param[in] ag    Assembly group
 *
 * @return the number of available slots
 */
uint64_t assembly_group_get_available_slots_count(const assembly_group_t *ag,
                                                  const storage_t *storage);

/**
 * Lookup an assembly volume in an assembly group.
 *
 * @param[in] ag    Assembly group
 * @param[in] uuid  UUID of assembly volume
 *
 * @return assembly volume if found, NULL otherwise
 */
assembly_volume_t *assembly_group_lookup_volume(const assembly_group_t *ag,
                                                const exa_uuid_t *uuid);

/**
 * Reserve several slots of an assembly group to an assembly volume
 *
 * @param[in]  ag        Assembly group
 * @param[in]  uuid      UUID for the assembly volume
 * @param[in]  nb_slots  Number of slots to allocate for the volume
 * @param[out] av        Assembly volume
 *
 * @attention which slots is unimportant, the first free group slots will
 *            be reserved.
 *
 * @return EXA_SUCCESS or a negative error code otherwise
 */
int assembly_group_reserve_volume(assembly_group_t *ag, const exa_uuid_t *uuid,
                                     uint64_t nb_slots, assembly_volume_t **av,
                                     storage_t *storage);

/**
 * Release all the slots of a volume.
 *
 * Don't use this function, use macro assembly_group_release_volume().
 *
 * @param[in]     ag        Assembly group
 * @param[in,out] av        Assembly volume
 * @param[in]     storage   Underlying storage
 */
void __assembly_group_release_volume(assembly_group_t *ag, assembly_volume_t *av,
                                        const storage_t *storage);

/** Release a volume and set it to NULL */
#define assembly_group_release_volume(ag, av, storage) \
    (__assembly_group_release_volume((ag), (av), (storage)), (av) = NULL)

/**
 * Change the number of allocated slots of a volume.
 * Automatically allocate or free slots if needed.
 *
 * @param[in]     ag            Assembly group
 * @param[in,out] av            Assembly volume
 * @param[in]     new_nb_slots  New number of slots
 * @param[in]     storage       The storage
 *
 * @return EXA_SUCCESS or a negative error code otherwise
 */
int assembly_group_resize_volume(assembly_group_t *ag, assembly_volume_t *av,
                                 uint64_t new_nb_slots, const storage_t *storage);

typedef enum { AG_HEADER_MAGIC = 0x66A33A11 } ag_header_magic_t;

#define AG_HEADER_FORMAT  1

typedef struct
{
    ag_header_magic_t magic;
    uint32_t format;
    uint32_t slot_size;
    uint32_t slot_width;
    uint32_t num_subspaces;
} ag_header_t;

int assembly_group_header_read(ag_header_t *header, stream_t *stream);

uint64_t assembly_group_serialized_size(const assembly_group_t *ag);

int assembly_group_serialize(const assembly_group_t *ag, stream_t *stream);
int assembly_group_deserialize(assembly_group_t *ag, const storage_t *storage,
                                  stream_t *stream);

/**
 * Dump an assembly group in user-friendly form to a stream.
 *
 * @param[in] ag      Assembly group
 * @param     stream  Stream to dump to
 *
 * @return 0 if successful, a negative error code otherwise
 */
int assembly_group_dump(const assembly_group_t *ag, stream_t *stream);

#endif  /* __ASSEMBLY_GROUP_H__ */
