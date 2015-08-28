
/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */



#include "common/include/exa_error.h"

#include "vrt/layout/sstriping/src/lay_sstriping.h"
#include "vrt/layout/sstriping/src/lay_sstriping_group.h"
#include "vrt/assembly/src/assembly_group.h"
#include "vrt/assembly/src/assembly_slot.h"

#define SLOT_DEBUG 0


/**
 * Converts a position in a given volume (virtual position) into a
 * real device : position (physical position). This function
 * is called for each request on a volume, and allows to compute the
 * physical location(s) of the data on the disks.
 *
 * @param[in]  volume  The volume being accessed
 *
 * @param[in]  vsector The sector being accessed in the volume
 *
 * @param[out] rdev    Real device containing the data
 *
 * @param[out] rsector Location in 'rd' of the data
 *
 * @note This function strongly assumes that accesses are made on
 * blocks, and that accesses accross real devices in a single block
 * are not possible.
 */
void
sstriping_volume2rdev(vrt_volume_t *volume, uint64_t vsector,
		      vrt_realdev_t **rdev, uint64_t *rsector)
{
    sstriping_group_t *lg = SSTRIPING_GROUP(volume->group);
    assembly_group_t *ag = &lg->assembly_group;
    unsigned int chunk_index;
    uint64_t offset;
    const slot_t *slot;
    uint64_t su;

    assembly_group_map_sector_to_slot(ag, volume->assembly_volume,
                                      lg->logical_slot_size,
                                      vsector, &slot, &offset);

    /* The physical layout is as follow:

       chunk idx   0      1      2      3      4      5
       +------+------+------+------+------+------+
       stripe 0 +  A   +  B   +  C   +  D   +  E   +  F   + slot 0
       stripe 1 +  G   +  H   +  I   +  J   +  K   +  L   +   |
       stripe 2 +  M   +  N   +  O   +  P   +  Q   +  R   +   |
       stripe 3 +  S   +  T   +  U   +  V   +  W   +  X   +   |
       stripe 4 +  a   +  b   +  c   +  d   +  e   +  f   + slot 1
       stripe 5 +  g   +  h   +  i   +  j   +  k   +  l   +   |
       stripe 6 +  m   +  n   +  o   +  p   +  q   +  r   +   |
       stripe 7 +  s   +  t   +  u   +  v   +  w   +  x   +   |
       +------+------+------+------+------+------+
    */

    /* Compute the index of the striping unit in the slot */
    su = offset / lg->su_size;

    /* Compute the offset within the striping unit */
    offset = offset % lg->su_size;

    /* Compute the index of the chunk in the slot */
    chunk_index = su % assembly_group_get_slot_width(ag);

    /* Compute the offset within the chunk */
    offset += (su / assembly_group_get_slot_width(ag)) * lg->su_size;

    assembly_slot_map_sector_to_rdev(slot, chunk_index, offset, rdev, rsector);
}
