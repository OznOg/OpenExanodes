/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_STRIPING_H__
#define __RAIN1_STRIPING_H__

#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_rdev.h"

#include "vrt/virtualiseur/include/vrt_volume.h"

/**
 * Converts a position in a given volume (virtual position) into the
 * number of the corresponding dirty zone, and the number of the 4kb
 * block containing the metadata corresponding to this dirty zone.
 *
 * @param[in]  rxg                  The rain1 group supporting the volume
 * @param[in]  av                   The assembly volume supporting the volume
 * @param[in]  vsector              The sector being accessed in the volume
 * @param[out] slot_index           The index of the slot corresponding to the volume sector
 * @param[out] dzone_index_in_slot  The index of the corresponding dirty zone in the slot
 *
 * @note This function strongly assumes that accesses are made on
 * blocks, and that accesses accross real devices in a single block
 * are not possible.
 */
void rain1_volume2dzone(const rain1_group_t *rxg,
                        const assembly_volume_t *av,
                        uint64_t vsector,
                        unsigned int *slot_index,
                        unsigned int *dzone_index_in_slot);


/**
 * Convert a logical position in the volume into an array of physical
 * locations on disks.
 *
 * @param[in]  rxg           The rain1 group supporting the volume
 * @param[in]  av            The assembly volume supporting the volume
 * @param[in]  vsector       Sector offset in the volume (logical position)
 * @param[out] rdev_loc      Array of rdev_location containing the data
 * @param[out] nb_rdev_loc   Number of elements in the "rdev_loc" array
 * @param[in]  max_rdev_loc  Maximum size of "rdev_loc" array
 *
 * @note This function strongly assumes that accesses are made on
 * blocks, and that accesses accross real devices in a single block
 * are not possible.
 */
void rain1_volume2rdev(const rain1_group_t *rxg,
                       const assembly_volume_t *av,
                       uint64_t vsector,
                       struct rdev_location *rdev_loc,
                       unsigned int *nb_rdev_loc,
                       unsigned int max_rdev_loc);

/**
 * Convert a logical position in a slot whole (data and metadata) space into an
 * array of physical locations on disks.
 *
 * @param[in]  rxg           The rain1 group supporting the volume
 * @param[in]  slot          The slot
 * @param[in]  sector        Sector offset in the whole slot logical space
 * @param[out] rdev_loc      Array of rdev_location containing the data
 * @param[out] nb_rdev_loc   Number of elements in the "rdev_loc" array
 * @param[in]  max_rdev_loc  Maximum size of "rdev_loc" array
 *
 */
void rain1_slot_raw2rdev(const rain1_group_t *rxg,
                         const slot_t *slot,
                         uint64_t ssector,
                         struct rdev_location *rdev_loc,
                         unsigned int *nb_rdev_loc,
                         unsigned int max_rdev_loc);

/**
 * Convert a logical position in a slot data space into an array of physical
 * locations on disks.
 *
 * @param[in]  rxg           The rain1 group supporting the volume
 * @param[in]  slot          The slot
 * @param[in]  sector        Sector offset in the slot logical space reserved for data
 * @param[out] rdev_loc      Array of rdev_location containing the data
 * @param[out] nb_rdev_loc   Number of elements in the "rdev_loc" array
 * @param[in]  max_rdev_loc  Maximum size of "rdev_loc" array
 *
 */
void rain1_slot_data2rdev(const rain1_group_t *rxg,
                          const slot_t *slot,
                          uint64_t ssector,
                          struct rdev_location *rdev_loc,
                          unsigned int *nb_rdev_loc,
                          unsigned int max_rdev_loc);


/**
 * Find the physical locations on disks where a dirty zone block must be
 * written.
 *
 * @param[in]  rxg           The rain1 group supporting the volume
 * @param[in]  slot          The slot the metadata block corresponds to
 * @param[in]  node_index    Index of the node the metadata block corresponds to
 * @param[out] rdev_loc      Array of rdev_location containing the data
 * @param[out] nb_rdev_loc   Number of elements in the "rdev_loc" array
 * @param[in]  max_rdev_loc  Maximum size of "rdev_loc" array
 *
 */
void rain1_dzone2rdev(const rain1_group_t *rxg,
                      const slot_t *slot,
                      unsigned int node_index,
                      struct rdev_location *rdev_loc,
                      unsigned int *nb_rdev_loc,
                      unsigned int max_rdev_loc);

#endif  /* __RAIN1_STRIPING_H__ */
