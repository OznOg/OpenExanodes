/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/clientd/src/nbd_blockdevice.h"

#include "os/include/os_atomic.h"
#include "os/include/os_mem.h"
#include "os/include/os_error.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_nbd_list.h"

typedef struct
{
    nbd_make_request_t *make_request;
    unsigned long long sector_count; /* size of the device in sectors */
    os_thread_mutex_t lock;
    int max_bio_size;     /** max number of sectors that this device suppports
                           * for an IO. (thus value is in sectors) */
    ndev_t *ndev;

    struct nbd_root_list bio_for_split;
    struct nbd_root_list bio_split;
} nbd_bdev_t;

static const char *nbd_blockdevice_name(const void *ctx)
{
    return ndev_get_name(((nbd_bdev_t *)ctx)->ndev);
}

static uint64_t nbd_blockdevice_get_sector_count(const void *ctx)
{
    return ((nbd_bdev_t *)ctx)->sector_count;
}

static int nbd_blockdevice_set_sector_count(void *ctx, uint64_t count)
{
    nbd_bdev_t *bdev = ctx;

    os_thread_mutex_lock(&bdev->lock);
    bdev->sector_count = count;
    os_thread_mutex_unlock(&bdev->lock);

    return 0;
}

/* FIXME a bench would be nice to tell if this wonderful value is not too small */
#define NB_BIO_IN_SPLIT_POOL 16

#define NB_BIO_PER_POOL 512

typedef struct
{
    blockdevice_io_t *bio_orig;
    nbd_bdev_t *bdev;
    os_atomic_t bio_waiting;
    int err;
} blockdevice_io_split_t;

static void bio_split_callback(blockdevice_io_t *bio, int err)
{
    blockdevice_io_split_t *split = bio->private_data;
    bool free_split_struct;

    EXA_ASSERT(os_atomic_read(&split->bio_waiting) > 0);

    if (err != 0)
        split->err = err;

    free_split_struct = os_atomic_dec_and_test(&split->bio_waiting);

    nbd_list_post(&split->bdev->bio_for_split.free, bio, -1);

    if (free_split_struct)
    {
        blockdevice_io_t *bio_orig = split->bio_orig;
        int err = split->err;

        nbd_list_post(&split->bdev->bio_split.free, split, -1);

        /* Careful: end_io() MUST be called AFTER nbd_list_post(split) because
         * the caller may destroy the bdev just after the end IO, and thus
         * making the nbd_list_post(split) fail as the list is freed. */
        blockdevice_end_io(bio_orig, err);
    }
}

static void bdev_submit_io(nbd_bdev_t *bdev, blockdevice_io_t *bio)
{
    char err = 0;

    os_thread_mutex_lock(&bdev->lock);

    if (bio->size > 0
        && bio->start_sector + BYTES_TO_SECTORS(bio->size) > bdev->sector_count)
        err = -EIO;

    os_thread_mutex_unlock(&bdev->lock);

    if (err != 0)
        blockdevice_end_io(bio, err);
    else
        bdev->make_request(bdev->ndev, bio);
}

/**
 * Split a bio in several part and submit it. The completion callback
 * will be called when all part of the bio will be done.
 * @param bio the bio to split
 */
static int nbd_blockdevice_submit_io(void *ctx, blockdevice_io_t *bio)
{
  nbd_bdev_t *bdev = ctx;
  uint64_t max_bio_size = bdev->max_bio_size;
  uint64_t bio_size_in_sector = BYTES_TO_SECTORS(bio->size);
  blockdevice_io_split_t *split;
  int len_sector = 0;
  int bv_off = 0;
  int split_bio_count = 0;

  if (bio_size_in_sector <= max_bio_size)
  {
      bdev_submit_io(bdev, bio);
      return 0;
  }

  split = nbd_list_remove(&bdev->bio_split.free, NULL, LISTWAIT);
  EXA_ASSERT(split != NULL);

  split->bio_orig = bio;
  split->bdev     = bdev;
  split->err      = 0;
  /* As we know the original size of the IO on the LU and the maximum size of
   * each IO, we can compute the number of bios that are needed.
   * - IO is bigger than max_bio_size -> split the IO in pieces of size max_bio_size.
   */
  os_atomic_set(&split->bio_waiting,
                quotient_ceil64(bio_size_in_sector, max_bio_size));

  while (len_sector < bio_size_in_sector)
  {
      blockdevice_io_t *bio_temp;
      uint64_t io_size;

      /* Set the I/O size to the biggest possible, unless we're at the
       * end of the parent IO and there's less to handle. */
      io_size = MIN(bio->size - bv_off, SECTORS_TO_BYTES(max_bio_size));

      bio_temp = nbd_list_remove(&bdev->bio_for_split.free, NULL, LISTWAIT);
      EXA_ASSERT(bio_temp != NULL);

      __blockdevice_submit_io(bio->bdev, bio_temp, bio->type,
                              bio->start_sector + len_sector,
                              (char *)bio->buf + bv_off, io_size,
                              bio->flush_cache, bio->bypass_lock,
                              split, bio_split_callback);

      bv_off += io_size;

      len_sector += BYTES_TO_SECTORS(io_size);

      split_bio_count++;
  }

  EXA_ASSERT(split_bio_count > 0);

  return 0;
}

static void bdev_delete(nbd_bdev_t *bdev)
{
    nbd_close_root(&bdev->bio_split);
    nbd_close_root(&bdev->bio_for_split);

    os_thread_mutex_destroy(&bdev->lock);

    os_free(bdev);
}

static int nbd_blockdevice_close(void *ctx)
{
    bdev_delete((nbd_bdev_t *)ctx);

    return 0;
}

static blockdevice_ops_t nbd_blockdevice_ops =
{
    .get_name_op = nbd_blockdevice_name,
    .get_sector_count_op = nbd_blockdevice_get_sector_count,
    .set_sector_count_op = nbd_blockdevice_set_sector_count,
    .submit_io_op = nbd_blockdevice_submit_io,
    .close_op = nbd_blockdevice_close
};


static nbd_bdev_t *bdev_create(nbd_make_request_t *make_request,
                               int max_bio_size, ndev_t *ndev)
{
    nbd_bdev_t *bd = NULL;

    bd = os_malloc(sizeof(nbd_bdev_t));
    EXA_ASSERT(bd != NULL);

    bd->make_request     = make_request;
    /* Size is changed later on when ndev is started */
    bd->sector_count = 0;
    bd->max_bio_size = max_bio_size;
    bd->ndev         = ndev;

    os_thread_mutex_init(&bd->lock);

    nbd_init_root(NB_BIO_IN_SPLIT_POOL, sizeof(blockdevice_io_split_t),
	          &bd->bio_split);
    nbd_init_root(NB_BIO_PER_POOL, sizeof(blockdevice_io_t),
	          &bd->bio_for_split);

    return bd;
}

int nbd_blockdevice_open(blockdevice_t **blockdevice,
                          blockdevice_access_t access,
                          int max_io_size,
                          nbd_make_request_t *make_request,
                          ndev_t *ndev)
{
    int err;
    nbd_bdev_t *bdev = bdev_create(make_request, max_io_size, ndev);

    if (bdev == NULL)
        return -ENOMEM;

    err = blockdevice_open(blockdevice, bdev, &nbd_blockdevice_ops, access);

    if (err != 0)
        bdev_delete(bdev);

    return err;
}
