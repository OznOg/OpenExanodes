/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __TEST_GROUP_ASSEMBLY_H__
#define __TEST_GROUP_ASSEMBLY_H__

#include "os/include/os_inttypes.h"

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/storage.h"

#define DEFAULT_SLOT_WIDTH 3
#define DEFAULT_HEURISTIC_NAME "max_simple"

struct vrt_realdev *read_rdevs_from_file(const char *filename,
                                         unsigned int *rdev_count);

storage_t *make_storage(struct vrt_realdev *rdevs, unsigned int rdev_count,
                        uint32_t chunk_size /* in sectors */);

struct assembly_group *make_assembly_group(struct vrt_realdev *rdevs,
                                           unsigned int rdev_count,
                                           const storage_t *sto,
                                           uint32_t chunk_size,
                                           uint32_t slot_width,
                                           const char *heuristic_name,
                                           bool verbose);

char *serialize_group_assembly_to_xml(assembly_group_t *ag,
                                      assembly_volume_t *av,
                                      const storage_t *sto,
                                      struct vrt_realdev *rdevs,
                                      unsigned int rdev_count);

/**
 * Compute the number of chunks per SPOF, based on realdevs.
 *
 * XXX This function currently assumes that node id <=> spof group id,
 * which is obviously wrong now that we have proper spof groups, but
 * since the realdev data files still don't know anything about that...
 *
 * @param[in]  rdevs        Array of realdevs
 * @param[in]  rdev_count   Number of entries in the rdevs array
 * @param[in]  chunk_size   Chunk size
 * @param[out] spof_chunks  Resulting number of chunks per spof
 * @param[out] spof_count   Number of entries in resulting array
 *
 * @return 0 if successful, a negative error code otherwise
 */
int compute_per_spof_chunks(const struct vrt_realdev *rdevs, unsigned rdev_count,
                            uint32_t chunk_size,
                            uint64_t **spof_chunks, unsigned *spof_count);

#endif /* __TEST_GROUP_ASSEMBLY_H__ */
