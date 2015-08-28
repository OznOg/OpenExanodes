/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/include/volume_blockdevice.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_request.h"

#include "blockdevice/include/blockdevice.h"

#include "os/include/os_mem.h"
#include "os/include/os_error.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_nbd_list.h"

typedef struct
{
    vrt_volume_t *volume;
    uint64_t su_size;

    os_thread_mutex_t lock;
    struct nbd_root_list bio_for_split;
    struct nbd_root_list bio_split;
} volume_bdev_t;

static const char *volume_blockdevice_name(const void *ctx)
{
    /* XXX? */
    return NULL;
}

static uint64_t volume_blockdevice_get_sector_count(const void *ctx)
{
    return ((volume_bdev_t *)ctx)->volume->size;
}

static int volume_blockdevice_set_sector_count(void *ctx, uint64_t count)
{
    /* This is done via vrt_volume_resize. */
    return -EPERM;
}

/* FIXME a bench would be nice to tell if this wonderful value is not too small */
#define NB_BIO_IN_SPLIT_POOL 160

#define NB_BIO_PER_POOL 512

typedef struct
{
    os_thread_mutex_t lock;
    blockdevice_io_t *bio_orig;
    volume_bdev_t *bdev;
    int bio_waiting;
    int err;
} blockdevice_io_split_t;

static void bio_split_callback(blockdevice_io_t *bio, int err)
{
    blockdevice_io_split_t *split = bio->private_data;
    int count;

    os_thread_mutex_lock(&split->lock);

    EXA_ASSERT(split->bio_waiting > 0);

    if (err != 0)
        split->err = err;

    split->bio_waiting--;
    count = split->bio_waiting;

    os_thread_mutex_unlock(&split->lock);

    nbd_list_post(&split->bdev->bio_for_split.free, bio, -1);

    if (count == 0)
    {
        blockdevice_io_t *bio_orig = split->bio_orig;
        int err = split->err;

        os_thread_mutex_destroy(&split->lock);

        nbd_list_post(&split->bdev->bio_split.free, split, -1);

        /* Careful: end_io() MUST be called AFTER nbd_list_post(split) because
         * the caller may destroy the bdev just after the end IO, and thus
         * making the nbd_list_post(split) fail as the list is freed. */
        blockdevice_end_io(bio_orig, err);
    }
}

static bool __io_fits_striping_unit(const blockdevice_io_t *bio, uint64_t su_size)
{
    int64_t end_sector;

    if (su_size == 0 || bio->size == 0)
        return true;

    end_sector = bio->start_sector + BYTES_TO_SECTORS(bio->size) - 1;
    return bio->start_sector / su_size == end_sector / su_size;
}

static void bdev_submit_io(volume_bdev_t *bdev, blockdevice_io_t *bio)
{
    char err = 0;

    EXA_ASSERT(__io_fits_striping_unit(bio, bdev->su_size));

    os_thread_mutex_lock(&bdev->lock);

    if (bio->size > 0
        && bio->start_sector + BYTES_TO_SECTORS(bio->size) > bdev->volume->size)
        err = -EIO;

    os_thread_mutex_unlock(&bdev->lock);

    if (err != 0)
        blockdevice_end_io(bio, err);
    else
        vrt_make_request(bdev->volume, bio);
}

/**
 * Split a bio in several part and submit it. The completion callback
 * will be called when all part of the bio will be done.
 * @param bio the bio to split
 */
static int volume_blockdevice_submit_io(void *ctx, blockdevice_io_t *bio)
{
  volume_bdev_t *bdev = ctx;
  uint64_t su_size   = bdev->su_size;
  uint64_t bio_size_in_sector = BYTES_TO_SECTORS(bio->size);
  blockdevice_io_split_t *split;
  int len_sector = 0;
  int bv_off = 0;
  int split_bio_count = 0, split_bio_waiting = 0;

  /* IO fits inside a striping unit: it's submittable unsplit. */
  if (__io_fits_striping_unit(bio, su_size))
  {
      bdev_submit_io(bdev, bio);
      return 0;
  }

  EXA_ASSERT(bio->size > 0);

  split = nbd_list_remove(&bdev->bio_split.free, NULL, LISTWAIT);
  EXA_ASSERT(split != NULL);

  os_thread_mutex_init(&split->lock);
  split->bio_orig = bio;
  split->bdev     = bdev;
  split->err      = 0;

  /* The IO overlaps multiple striping units, so it must be split
   * in IOs fitting inside them.
   */
  split->bio_waiting = quotient_ceil64(bio_size_in_sector + bio->start_sector, su_size)
                       - bio->start_sector / su_size;

  split_bio_waiting = split->bio_waiting;

  while (len_sector < bio_size_in_sector)
  {
      blockdevice_io_t *bio_temp;
      int io_size;

      /* FIXME stop messing up with sectors and bytes here... */

      /* Make sure the IO doesn't cross the SU bound. */
      io_size = MIN(BYTES_TO_SECTORS(bio->size - bv_off),
                    su_size - (bio->start_sector + len_sector) % su_size);

      bio_temp = nbd_list_remove(&bdev->bio_for_split.free,
                                 NULL, LISTWAIT);
      EXA_ASSERT(bio_temp != NULL);

      __blockdevice_submit_io(bio->bdev, bio_temp, bio->type, bio->start_sector
                          + len_sector, (char *)bio->buf + bv_off,
                          SECTORS_TO_BYTES(io_size), bio->flush_cache,
                          bio->bypass_lock, split, bio_split_callback);

      bv_off += SECTORS_TO_BYTES(io_size);

      len_sector += io_size;

      split_bio_count++;
  }

  EXA_ASSERT(split_bio_count > 0);

  /* XXX this is sanity check, should be useless if the code is perfect, but
   * as it is quite difficult to test, I keep it here. This could/should be
   * removed when we think this is bugfree */
  EXA_ASSERT_VERBOSE(split_bio_count == split_bio_waiting,
                     "Error in bio split: expected=%d performed=%d."
                     " size=%"PRIu64" su_size=%" PRIu64,
                     split_bio_waiting, split_bio_count, bio->size,
                     su_size);

  return 0;
}

static void bdev_delete(volume_bdev_t *bdev)
{
    nbd_close_root(&bdev->bio_split);
    nbd_close_root(&bdev->bio_for_split);

    os_thread_mutex_destroy(&bdev->lock);

    os_free(bdev);
}

static int volume_blockdevice_close(void *ctx)
{
    bdev_delete((volume_bdev_t *)ctx);

    return 0;
}

static blockdevice_ops_t volume_blockdevice_ops =
{
    .get_name_op = volume_blockdevice_name,
    .get_sector_count_op = volume_blockdevice_get_sector_count,
    .set_sector_count_op = volume_blockdevice_set_sector_count,
    .submit_io_op = volume_blockdevice_submit_io,
    .close_op = volume_blockdevice_close
};

static volume_bdev_t *volume_bdev_create(vrt_volume_t *volume)
{
    volume_bdev_t *bd = NULL;
    uint64_t su_size = volume->group->layout->get_su_size(volume->group->layout_data);

    bd = os_malloc(sizeof(volume_bdev_t));
    EXA_ASSERT(bd != NULL);

    bd->volume  = volume;
    bd->su_size = su_size;

    os_thread_mutex_init(&bd->lock);

    nbd_init_root(NB_BIO_IN_SPLIT_POOL, sizeof(blockdevice_io_split_t),
	          &bd->bio_split);
    nbd_init_root(NB_BIO_PER_POOL, sizeof(blockdevice_io_t),
	          &bd->bio_for_split);

    return bd;
}

int volume_blockdevice_open(blockdevice_t **blockdevice, vrt_volume_t *volume,
                            blockdevice_access_t access)
{
    volume_bdev_t *bdev = volume_bdev_create(volume);
    int err;

    if (bdev == NULL)
        return -ENOMEM;

    err = blockdevice_open(blockdevice, bdev, &volume_blockdevice_ops, access);

    if (err != 0)
        bdev_delete(bdev);

    return err;
}
