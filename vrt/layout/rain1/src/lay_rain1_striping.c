/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/**
 * @file
 *
 * This source file contains the functions that are responsible to
 * compute the mapping between a logical position and the physical
 * locations on disks. Two important functions are implemented here:
 * rain1_volume2rdev() and rain1_group2rdev().
 * Most of the logic of the rain1 placement is located in
 * rain1_group2rdev() which apply the striping formula, the
 * replication formula, the distributed shift formula and the sparing
 * formula to compute the physical locations of the data.
 */

#include <string.h>

#include "log/include/log.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_math.h"
#include "common/include/uuid.h"

#include "os/include/os_inttypes.h"

#include "vrt/assembly/src/assembly_slot.h"
#include "vrt/assembly/src/assembly_group.h"

#include "vrt/layout/rain1/src/lay_rain1_metadata.h" /* for rain1_per_group_dzone_count (only assert) */
#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_status.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"


/**
 * Stripe data across the slot chunks
 *
 * chunk idx:    0      1      2      3   
 *            +------+------+------+------+
 * stripe 0   +  0   +  1   +  2   +  3   +
 * stripe 1   +  4   +  5   +  6   +  7   +
 * stripe 2   +  8   +  9   +  10  +  11  +
 * stripe 3   +  12  +  13  +  14  +  15  +
 *            +---------------------------+
 *
 * In the previous slot figure, the slot contains 4 chunks, thus the stripe
 * width is 4. Striping unit #11 is located on stripe #2 and its chunk index
 * is 3.
 *
 * @param[in]  sector        Sector position in the slot
 * @param[in]  su_size       Striping unit size (in sectors)
 * @param[in]  stripe_width  Width of the stripe
 * @param[out] offset        Computed offset in the stripe
 * @param[out] stripe        Computed index of the stripe
 * @param[out] chunk_idx     Computed index of the chunk inside the stripe
 */
static inline void
rain1_striping(uint64_t sector, unsigned int su_size, unsigned int stripe_width,
               unsigned int *offset, unsigned int *stripe, unsigned int *chunk_idx)
{
    *offset    = sector % su_size;
    *stripe    = sector / su_size / stripe_width;
    *chunk_idx = (sector / su_size) % stripe_width;
}


/**
 * Replication function. The purpose of the replication function is to
 * duplicate a stripe: for a given non-replicated stripe, the function
 * returns two stripes (an original stripe and a mirror stripe)
 * that will protect each other.
 *
 * @param[in]  stripe      Stripe index
 * @param[out] stripe1     Stripe index of the original stripe
 * @param[out] stripe2     Stripe index of the mirror stripe
 * @param[in]  blended     Whether to use blended stripes or not
 * @param[in]  nb_stripes  Number of stripes per slot
 */
static inline void
rain1_replication(unsigned int stripe,
                  unsigned int *stripe1,
                  unsigned int *stripe2,
                  uint32_t blended,
                  uint32_t nb_stripes)
{
    if (blended)
    {
        /* Here, the data and its copy are interlaced stripe by stripe. An
           example of physical layout could be:

           chunk idx   0      1      2      3      4      5
	   +------+------+------+------+------+------+
           stripe 0 +  A   +  B   +  C   +  D   +  E   +  F   + slot 0
           stripe 1 +  f   +  a   +  b   +  c   +  d   +  e   +   |
           stripe 2 +  G   +  H   +  I   +  J   +  K   +  L   +   |
           stripe 3 +  k   +  l   +  g   +  h   +  i   +  j   +   |
           stripe 4 +  M   +  N   +  O   +  P   +  Q   +  R   + slot 1
           stripe 5 +  p   +  q   +  r   +  m   +  n   +  o   +   |
           stripe 6 +  S   +  T   +  U   +  V   +  W   +  X   +   |
           stripe 7 +  u   +  v   +  w   +  x   +  s   +  t   +   |
	   +------+------+------+------+------+------+
        */
        *stripe1 = stripe * 2;
        *stripe2 = (*stripe1) + 1;
    }
    else
    {
        /* Here, the data are in the first half of the slot, while the
           mirrors are in the second half of the slot.

           chunk idx   0      1      2      3      4      5
	   +------+------+------+------+------+------+
           stripe 0 +  A   +  B   +  C   +  D   +  E   +  F   + slot 0
           stripe 1 +  G   +  H   +  I   +  J   +  K   +  L   +   |
           stripe 2 +  f   +  a   +  b   +  c   +  d   +  e   +   |
           stripe 3 +  k   +  l   +  g   +  h   +  i   +  j   +   |
           stripe 4 +  M   +  N   +  O   +  P   +  Q   +  R   + slot 1
           stripe 5 +  S   +  T   +  U   +  V   +  W   +  X   +   |
           stripe 6 +  p   +  q   +  r   +  m   +  n   +  o   +   |
           stripe 7 +  u   +  v   +  w   +  x   +  s   +  t   +   |
	   +------+------+------+------+------+------+
        */
        *stripe1 = stripe;
        *stripe2 = (*stripe1) + nb_stripes / 2;
    }
    EXA_ASSERT_VERBOSE(*stripe1 < nb_stripes,
                       "Original stripe (%u) out of range (nb_stripes = %" PRIu32 ")",
                       *stripe1, nb_stripes);
    EXA_ASSERT_VERBOSE(*stripe2 < nb_stripes,
                       "Mirror stripe (%u) out of range (nb_stripes = %" PRIu32 ")",
                       *stripe2, nb_stripes);
}


/**
 * Shift the index of the chunk in the mirror stripe (i.e. the second
 * chunk). The purpose of this operation is to place the replicas on
 * two different chunks.
 * The shift offset is not constant: it depends on the index of the
 * original stripe.
 *
 * @param[in]  chunk1_idx    Index of the chunk that holds the
 *                           original data
 * @param[in]  stripe        Index of the original stripe
 * @param[in]  stripe_width  Width of the stripe
 * @param[out] chunk2_idx    Computed index of the chunk that holds
 *                           the mirror data
 */
static inline void
rain1_distributed_shift(unsigned int chunk1_idx,
                        unsigned int stripe,
                        unsigned int stripe_width,
                        unsigned int *chunk2_idx)
{
    EXA_ASSERT(stripe_width > 1);
    *chunk2_idx = (chunk1_idx + 1 + stripe % (stripe_width - 1)) % stripe_width;
    EXA_ASSERT(*chunk2_idx != chunk1_idx);
}

static void slot_to_rdev_location(const rain1_group_t *rxg,
                                  const slot_t *slot, uint64_t ssector,
                                  struct rdev_location *rdev_loc,
                                  unsigned int *nb_rdev_loc,
                                  unsigned int max_rdev_loc)
{
    unsigned int offset;        /* Offset within the stripe */
    unsigned int stripe_width;  /* Stripe width */
    unsigned int nb_stripes;    /* Number of logical stripes in the slot */
    unsigned int stripe;        /* Index of the stripe in the slot */
    int i;

    /* Indexes of the replica chunks in the slot */
    unsigned int replica_chunk[2];

    /* Indexes of the replica stripes */
    unsigned int replica_stripe[2];

    EXA_ASSERT_VERBOSE(max_rdev_loc >= 2, "Not enough rdev locations");
    EXA_ASSERT(ssector < rxg->logical_slot_size);

    /* FIXME: we should assert that we do not write on free slots but since the
     * metadata wiping is done on all the slot (even free) we can't
     */
    memset(rdev_loc, 0, sizeof(struct rdev_location) * max_rdev_loc);

    stripe_width = slot->width;
    EXA_ASSERT(stripe_width > 1);

    EXA_ASSERT(nb_rdev_loc != NULL);
    *nb_rdev_loc = 0;

    /* apply striping formula */
    rain1_striping(ssector, rxg->su_size, stripe_width, &offset,
		   &stripe, &replica_chunk[0]);

    nb_stripes = rxg->logical_slot_size / (stripe_width * rxg->su_size) * 2;
    /* *2 because of the mirroring of data: there are actually twice as much
     * physical stripes thanlogical stripes */

    EXA_ASSERT(stripe < nb_stripes / 2);

    /* apply replication formula */
    rain1_replication(stripe, &replica_stripe[0], &replica_stripe[1],
		      rxg->blended_stripes, nb_stripes);

    /* apply distributed shift formula */
    rain1_distributed_shift(replica_chunk[0], stripe,
			    stripe_width, &replica_chunk[1]);

    /* !!! const cast !!!
     * allows us to take the lock while guaranteeing that we dont alter the structure */
    os_thread_rwlock_rdlock((os_thread_rwlock_t *)&rxg->status_lock);

    for (i = 0; i < 2; i++)
    {
	unsigned int chunk;
	struct vrt_realdev *rdev;
	uint64_t rdev_sector;
        sync_tag_t max_sync_tag = SYNC_TAG_ZERO;

	/* Store the current chunk index in the chain of chunk indexes */
	chunk = (replica_chunk[i] + stripe) % slot->width;

	assembly_slot_map_sector_to_rdev(slot, chunk,
			replica_stripe[i] * rxg->su_size + offset,
			&rdev, &rdev_sector);
	EXA_ASSERT(rdev);

	/* we compute the maximum outdate tag in the chain of sparing 'correction'
	 * that leads to this position. This maximum tag is used to find out if this
	 * position is should have been replicated while it was down.
	 */
	max_sync_tag = sync_tag_max(max_sync_tag, RAIN1_REALDEV(rxg, rdev)->sync_tag);

	/* Skip this replica position if the rain1 layout MUST NOT
	 * write on this position.
	 * We ensure that all the returned positions are writable.
	 */
	if (!rain1_rdev_is_writable(rxg, rdev))
		continue;

	/* Add the writable replica to the replica location list */
	EXA_ASSERT_VERBOSE((*nb_rdev_loc) < max_rdev_loc,
			"The replica location list is too small.");

	rdev_loc[*nb_rdev_loc].rdev = rdev;
	rdev_loc[*nb_rdev_loc].sector = rdev_sector;

	rdev_loc[*nb_rdev_loc].size = rxg->su_size - offset;
	rdev_loc[*nb_rdev_loc].uptodate =
		rain1_rdev_is_uptodate(RAIN1_REALDEV(rxg, rdev), rxg->sync_tag);
	rdev_loc[*nb_rdev_loc].never_replicated =
		sync_tag_is_greater(max_sync_tag, RAIN1_REALDEV(rxg, rdev)->sync_tag);

	(*nb_rdev_loc)++;
    }

    os_thread_rwlock_unlock((os_thread_rwlock_t *)&rxg->status_lock);
}

void rain1_volume2dzone(const rain1_group_t *rxg,
                        const assembly_volume_t *av,
                        uint64_t vsector,
                        unsigned int *slot_index,
                        unsigned int *dzone_index_in_slot)
{
    uint64_t offset_in_slot;

    /* Convert the logical position on the volume to the logical
     * position in the slot.
     */
    assembly_volume_map_sector_to_slot(av, rain1_group_get_slot_data_size(rxg), vsector,
                                       slot_index, &offset_in_slot);

    *dzone_index_in_slot = offset_in_slot / rxg->dirty_zone_size;

    EXA_ASSERT(*dzone_index_in_slot < rain1_group_get_dzone_per_slot_count(rxg));
}

void rain1_slot_raw2rdev(const rain1_group_t *rxg,
                         const slot_t *slot,
                         uint64_t ssector,
                         struct rdev_location *rdev_loc,
                         unsigned int *nb_rdev_loc,
                         unsigned int max_rdev_loc)
{
    EXA_ASSERT(slot != NULL);

    slot_to_rdev_location(rxg, slot, ssector,
                          rdev_loc, nb_rdev_loc, max_rdev_loc);
}

void rain1_slot_data2rdev(const rain1_group_t *rxg,
                          const slot_t *slot,
                          uint64_t ssector,
                          struct rdev_location *rdev_loc,
                          unsigned int *nb_rdev_loc,
                          unsigned int max_rdev_loc)
{
    EXA_ASSERT(ssector < rain1_group_get_slot_data_size(rxg));

    rain1_slot_raw2rdev(rxg, slot,
                        ssector + rain1_group_get_slot_metadata_size(rxg),
                        rdev_loc, nb_rdev_loc, max_rdev_loc);
}

static void rain1_slot_metadata2rdev(const rain1_group_t *rxg,
                                     const slot_t *slot,
                                     uint64_t ssector,
                                     struct rdev_location *rdev_loc,
                                     unsigned int *nb_rdev_loc,
                                     unsigned int max_rdev_loc)
{
    EXA_ASSERT(ssector < rain1_group_get_slot_metadata_size(rxg));

    rain1_slot_raw2rdev(rxg, slot, ssector,
                        rdev_loc, nb_rdev_loc, max_rdev_loc);
}

void rain1_volume2rdev(const rain1_group_t *rxg,
                       const assembly_volume_t *av,
                       uint64_t vsector,
                       struct rdev_location *rdev_loc,
                       unsigned int *nb_rdev_loc,
                       unsigned int max_rdev_loc)
{
    uint64_t ssector;           /* Logical position in the slot */
    const slot_t *slot;

    assembly_group_map_sector_to_slot(&rxg->assembly_group, av,
                                      rain1_group_get_slot_data_size(rxg),
                                      vsector, &slot, &ssector);

    rain1_slot_data2rdev(rxg, slot, ssector,
                         rdev_loc, nb_rdev_loc, max_rdev_loc);
}


void rain1_dzone2rdev(const rain1_group_t *rxg,
                      const slot_t *slot,
                      unsigned int node_index,
                      struct rdev_location *rdev_loc,
                      unsigned int *nb_rdev_loc,
                      unsigned int max_rdev_loc)
{
    uint64_t ssector = node_index * BYTES_TO_SECTORS(METADATA_BLOCK_SIZE);

    rain1_slot_metadata2rdev(rxg, slot, ssector,
                             rdev_loc, nb_rdev_loc, max_rdev_loc);
}
