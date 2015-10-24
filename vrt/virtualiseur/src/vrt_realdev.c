/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/src/vrt_module.h"

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_msg.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_layout.h"

#include "vrt/common/include/narrowed_stream.h"
#include "vrt/common/include/checksum_stream.h"

#include "nbd/clientd/include/nbd_clientd.h" /* for client_get_blockdevice */

#include "blockdevice/include/blockdevice_stream.h"

#include "log/include/log.h"

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_math.h"

#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Size of the blockdevice stream cache, in bytes */
#define __CACHE_SIZE  SECTORS_TO_BYTES(4)

/**
 * Get the size of a block device (in sectors).
 *
 * @param blockdevice  Blockdevice
 *
 * @return size aligned on the block size of the VRT
 */
static uint64_t __get_block_aligned_bdev_size(blockdevice_t *blockdevice)
{
    uint64_t full_size = blockdevice_get_sector_count(blockdevice);
    return ALIGN_INF(full_size, BYTES_TO_SECTORS(VRT_BLOCK_SIZE), uint64_t);
}

static uint64_t __usable_size(uint64_t size)
{
    if (size < VRT_SB_AREA_SIZE)
        return 0;

    return size - VRT_SB_AREA_SIZE;
}

/* FIXME Merge into vrt_rdev_free()? */
static void rdev_cleanup_chunks(vrt_realdev_t *rdev)
{
    extent_list_free(rdev->chunks.free_chunks);
    rdev->chunks.total_chunks_count = 0;
    rdev->chunks.free_chunks_count = 0;
}

uint64_t rdev_chunk_based_size(const vrt_realdev_t *rdev)
{
    EXA_ASSERT(rdev != NULL);
    return (uint64_t)rdev->chunks.chunk_size * rdev->chunks.total_chunks_count;
}

static void __get_superblock_data_range(int position, uint64_t *start, uint64_t *end)
{
    /* Each superblock is alloted half of the VRT's superblock area.
       The data part is a half minus the size of the superblock header. */
    uint64_t per_sb_data_size =
        (SECTORS_TO_BYTES(VRT_SB_AREA_SIZE) - 2 * sizeof(superblock_header_t)) / 2;

    *start = 2 * sizeof(superblock_header_t) + position * per_sb_data_size;
    *end = *start + per_sb_data_size - 1;
}

static void vrt_rdev_close_superblock_streams(vrt_realdev_t *rdev)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        stream_close(rdev->checksum_sb_streams[i]);
        stream_close(rdev->sb_data_streams[i]);
    }

    stream_close(rdev->raw_sb_stream);
}

static int vrt_rdev_open_superblock_streams(vrt_realdev_t *rdev)
{
    stream_access_t access = rdev->local ? STREAM_ACCESS_RW : STREAM_ACCESS_READ;
    int i;
    int err;

    err = blockdevice_stream_on(&rdev->raw_sb_stream, rdev->blockdevice,
                                __CACHE_SIZE, access);
    if (err != 0)
        return err;

    for (i = 0; i < 2; i++)
    {
        uint64_t start, end;

        __get_superblock_data_range(i, &start, &end);

        err = narrowed_stream_open(&rdev->sb_data_streams[i],
                                   rdev->raw_sb_stream, start, end, access);
        if (err != 0)
        {
            vrt_rdev_close_superblock_streams(rdev);
            return err;
        }

        err = checksum_stream_open(&rdev->checksum_sb_streams[i],
                                   rdev->sb_data_streams[i]);
        if (err != 0)
        {
            vrt_rdev_close_superblock_streams(rdev);
            return err;
        }
    }

    return EXA_SUCCESS;
}

/**
 * Create a new real dev.
 *
 * @param group       Group in which the real device will be added
 * @param node_id     ID of the node where the device is attached
 * @param uuid        UUID of the real device in the VRT
 * @param nbd_uuid    UUID of the real device in the NBD
 * @param local       true is the rdev is local
 * @param up          true if Admind considers the device as UP
 *
 * @return            a valid vrt_realdev
 */
struct vrt_realdev *vrt_rdev_new(exa_nodeid_t node_id,
                                 spof_id_t spof_id, const exa_uuid_t *uuid,
                                 const exa_uuid_t *nbd_uuid, int index,
                                 bool local, bool up)
{
    struct vrt_realdev *rdev;

    exalog_debug("adding rdev " UUID_FMT ": status = %s",
                 UUID_VAL(uuid), up ? "UP" : "DOWN");

    rdev = os_malloc(sizeof(struct vrt_realdev));
    if (rdev == NULL)
	return NULL;

    memset(rdev, 0, sizeof(struct vrt_realdev));

    /* FIXME It would be fucking great to have the fields initialized in the
             same order as they appear in the structure definition. */

    rdev->local = local;

    rdev->node_id = node_id;
    rdev->spof_id = spof_id;
    rdev->index   = index;

    /* Initialize the device status */
    rdev->up = up;
    rdev->corrupted = FALSE;
    rdev->real_size = 0;

    /* Initialize the its superblock info */
    uuid_copy(&rdev->uuid, uuid);
    uuid_copy(&rdev->nbd_uuid, nbd_uuid);

    rdev->chunks.chunk_size = 0;
    rdev->chunks.total_chunks_count = 0;
    rdev->chunks.free_chunks_count = 0;
    rdev->chunks.free_chunks = NULL;

    rdev->raw_sb_stream = NULL;

    rdev->sb_data_streams[0] = NULL;
    rdev->sb_data_streams[1] = NULL;

    rdev->checksum_sb_streams[0] = NULL;
    rdev->checksum_sb_streams[1] = NULL;

    return rdev;
}

void __vrt_rdev_free(struct vrt_realdev *rdev)
{
    rdev_cleanup_chunks(rdev);
    vrt_rdev_close_superblock_streams(rdev);

    os_free(rdev);
}

int vrt_rdev_set_real_size(struct vrt_realdev *rdev, uint64_t size)
{
    EXA_ASSERT(rdev->up);
    EXA_ASSERT(rdev->blockdevice != NULL);

    if (__usable_size(size) <= 0)
    {
        exalog_error("rdev " UUID_FMT " is too small (%" PRIu64
                     " sectors) to store the superblocks",
                     UUID_VAL(&rdev->uuid), size);
        return -VRT_ERR_RDEV_TOO_SMALL;
    }

    rdev->real_size = size;

    return EXA_SUCCESS;
}

int vrt_rdev_open(struct vrt_realdev *rdev)
{
    int err;

    rdev->blockdevice = client_get_blockdevice(&rdev->nbd_uuid);

    if (rdev->blockdevice == NULL)
    {
        exalog_error("Could not open device "UUID_FMT, UUID_VAL(&rdev->uuid));
        return -ENODEV;
    }

    err = vrt_rdev_open_superblock_streams(rdev);
    if (err != 0)
        return err;

    if (rdev->up)
    {
        uint64_t size = __get_block_aligned_bdev_size(rdev->blockdevice);
        err = vrt_rdev_set_real_size(rdev, size);

        if (err != 0)
            return err;
    }

    return EXA_SUCCESS;
}

/**
 * Ask the exclusion of a real device. This function is used in the "device
 * down" procedure. It places the given real device in the 'down' state and
 * asks the layout if it can still process requests without this device. If
 * not, group is put in the ERROR state, and an error is returned.
 *
 * @param[in] rdev    The real device to put in the DOWN state
 */
void vrt_rdev_down(struct vrt_realdev *rdev)
{
    rdev->real_size = 0;
    rdev->up = FALSE;
}

/**
 * Tells that a unusable device is now available again. We'll put it
 * in the EXA_REALDEV_UPDATING state, which doesn't mean we can safely
 * use it, but that a rebuilding process must take place before
 * changing the status to EXA_REALDEV_OK.
 *
 * @param[in] rdev       The real device which is now available again
 *
 * @return    always EXA_SUCCESS
 */
int
vrt_rdev_up(struct vrt_realdev *rdev)
{
    uint64_t size;
    uint64_t required_size;
    int err;

    /* Admind can send an up message even if the device is not down */
    if (rdev_is_ok(rdev))
	return EXA_SUCCESS;

    err = vrt_rdev_open(rdev);
    if (err != EXA_SUCCESS)
        return err;

    rdev->up = TRUE;

    size = __get_block_aligned_bdev_size(rdev->blockdevice);
    err = vrt_rdev_set_real_size(rdev, size);
    if (err != EXA_SUCCESS)
        return err;

    /* Check that the size of the device correspond to the real size of the
     * device.
     * This test is also done at group start (see vrt_group_start)
     *
     * FIXME: This kind of verification could be done by the service in
     * charge of the devices
     */
    required_size = rdev_chunk_based_size(rdev);
    if (vrt_realdev_get_usable_size(rdev) < required_size)
    {
	/* XXX Duplicate: same error in vrt_group_start() */
        exalog_error("Real size of device "UUID_FMT" is too small: %"PRIu64" < %"PRIu64,
                     UUID_VAL(&rdev->uuid), vrt_realdev_get_usable_size(rdev),
                     required_size);
        rdev->corrupted = TRUE;
        rdev->real_size = 0;

        return EXA_SUCCESS;
    }

    rdev->corrupted = FALSE;


    return EXA_SUCCESS;
}

int vrt_rdev_replace(struct vrt_realdev *rdev, const exa_uuid_t *new_rdev_uuid)
{
    blockdevice_t *new_blockdevice;
    int err;

    if (rdev_is_ok(rdev))
    {
	exalog_error("Bad rdev status %d", rdev_get_compound_status(rdev));
	return -VRT_ERR_CANT_DGDISKRECOVER;
    }

    new_blockdevice = client_get_blockdevice(new_rdev_uuid);
    if (new_blockdevice == NULL)
    {
        exalog_error("Could not open device "UUID_FMT, UUID_VAL(new_rdev_uuid));
        return -ENODEV;
    }

    if (__get_block_aligned_bdev_size(new_blockdevice)
        < __get_block_aligned_bdev_size(rdev->blockdevice))
        return -VRT_ERR_RDEV_TOO_SMALL;

    rdev->blockdevice = new_blockdevice;

    /* re-open the superblock stream */
    vrt_rdev_close_superblock_streams(rdev);
    err = vrt_rdev_open_superblock_streams(rdev);
    if (err != 0)
        return err;

    uuid_copy(&rdev->nbd_uuid, new_rdev_uuid);

    return EXA_SUCCESS;
}


/**
 * Ask the local NBD to lock writes on a range of sectors for a given
 * device. It only works on local devices.
 *
 * @param[in] rdev   The real device to lock
 * @param[in] start  The starting sector
 * @param[in] size   The size in sectors
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
int
vrt_rdev_lock_sectors(struct vrt_realdev *rdev, unsigned long start,
                      unsigned long size)
{
    EXA_ASSERT (rdev_is_local (rdev));
    return vrt_msg_nbd_lock(&rdev->nbd_uuid, start, size, TRUE);
}


/**
 * Ask the local NBD to unlock writes on a range of sectors for a
 * given device. It only works on local devices.
 *
 * @param[in] rdev   The real device to unlock
 * @param[in] start  The starting sector
 * @param[in] siz    The size in sectors
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
int
vrt_rdev_unlock_sectors(struct vrt_realdev *rdev, unsigned long start,
                        unsigned long size)
{
    EXA_ASSERT (rdev_is_local (rdev));
    return vrt_msg_nbd_lock(&rdev->nbd_uuid, start, size, FALSE);
}


exa_realdev_status_t rdev_get_compound_status(const struct vrt_realdev *rdev)
{
    EXA_ASSERT(rdev);

    if (!rdev->up)
	return EXA_REALDEV_DOWN;
    else if (rdev->corrupted)
	return EXA_REALDEV_ALIEN;
    else
	return EXA_REALDEV_OK;
}


exa_nodeid_t vrt_rdev_get_nodeid(const struct vrt_realdev *rdev)
{
    EXA_ASSERT(rdev);

    return rdev->node_id;
}

uint64_t vrt_realdev_get_usable_size(const vrt_realdev_t *rdev)
{
    return __usable_size(rdev->real_size);
}

static int __read_sb_headers(vrt_realdev_t *rdev, superblock_header_t headers[2])
{
    int err;

    err = stream_rewind(rdev->raw_sb_stream);
    if (err != 0)
        return err;

    return rdev_superblock_header_read_both(headers, rdev->raw_sb_stream);
}

static int __write_sb_header(vrt_realdev_t *rdev, const superblock_header_t *header)
{
    uint64_t ofs;
    int w;
    int err;

    ofs = header->position * sizeof(superblock_header_t);
    err = stream_seek(rdev->raw_sb_stream, ofs, STREAM_SEEK_FROM_BEGINNING);
    if (err != 0)
        return err;

    w = stream_write(rdev->raw_sb_stream, header, sizeof(superblock_header_t));
    if (w < 0)
        return w;
    else if (w != sizeof(superblock_header_t))
        return -EIO;

    return 0;
}

static int __check_sb_header(const superblock_header_t *header, int position)
{
    int i;

    if (header->magic != SUPERBLOCK_HEADER_MAGIC)
    {
        exalog_error("Invalid magic in superblock %d", position);
        return -VRT_ERR_SB_MAGIC;
    }

    if (header->format != SUPERBLOCK_HEADER_FORMAT)
    {
        exalog_error("Unknown format %d in superblock %d", header->format, position);
        return -VRT_ERR_SB_FORMAT;
    }

    if (header->position != position)
    {
        exalog_error("Invalid position %d in superblock %d", header->position,
                     position);
        return -VRT_ERR_SB_CORRUPTION;
    }

    if (header->reserved1 != 0)
    {
        exalog_error("Invalid reserved field 0x%08x in superblock %d",
                     header->reserved1, position);
        return -VRT_ERR_SB_CORRUPTION;
    }

    if (header->data_size > header->data_max_size)
    {
        exalog_error("Data size (%"PRIu64" bytes) in superblock %d"
                     " larger than alloted area (%"PRIu64" bytes)",
                     header->data_size, position, header->data_max_size);
        return -VRT_ERR_SB_CORRUPTION;
    }

    for (i = 0; i < sizeof(header->reserved2); i++)
        if (header->reserved2[i] != 0)
        {
            exalog_error("Invalid data 0x%0x in reserved field in superblock %d",
                         header->reserved2[i], position);
            return -VRT_ERR_SB_CORRUPTION;
        }

    return 0;
}

int vrt_rdev_create_superblocks(vrt_realdev_t *rdev)
{
    superblock_header_t header;
    int i, err;

    for (i = 0; i < 2; i++)
    {
        uint64_t start, end;

        __get_superblock_data_range(i, &start, &end);

        header.magic = SUPERBLOCK_HEADER_MAGIC;
        header.format = SUPERBLOCK_HEADER_FORMAT;
        header.position = i;
        header.reserved1 = 0;
        header.sb_version = 0;
        header.data_max_size = end - start + 1;
        header.data_offset = start;
        header.data_size = 0;
        header.checksum = 0;
        memset(header.reserved2, 0, sizeof(header.reserved2));

        err = __write_sb_header(rdev, &header);
        if (err != 0)
            return err;
    }

    return stream_flush(rdev->raw_sb_stream);
}

int vrt_rdev_begin_superblock_write(vrt_realdev_t *rdev, uint64_t old_version,
                                    uint64_t new_version, superblock_write_op_t *op)
{
    superblock_header_t headers[2];
    int write_position;
    int i;
    int err;

    err = __read_sb_headers(rdev, headers);
    if (err != 0)
        return err;

    write_position = -1;
    for (i = 0; i < 2; i++)
    {
        if (__check_sb_header(&headers[i], i) != 0)
            continue;

        /* We want to keep the old version => pick the header whose version
           is *not* old_version (or a header whose version is zero, ie none) */
        if (headers[i].sb_version == 0 || headers[i].sb_version != old_version)
        {
            write_position = i;
            break;
        }
    }

    if (write_position != 0 && write_position != 1)
    {
        exalog_error("No superblock available for writing version %"PRIu64,
                     new_version);
        return -VRT_ERR_SB_CORRUPTION;
    }

    op->header = headers[write_position];
    op->new_sb_version = new_version;
    op->stream = rdev->checksum_sb_streams[write_position];

    /* Rewind so that the stream is ready for writing. This also has the
       effect of resetting the checksum stream's checksum. */
    return stream_rewind(op->stream);
}

int vrt_rdev_end_superblock_write(vrt_realdev_t *rdev, superblock_write_op_t *op)
{
    int err;

    op->header.sb_version = op->new_sb_version;
    op->header.data_size = checksum_stream_get_size(op->stream);
    op->header.checksum = checksum_stream_get_value(op->stream);

    err =  __write_sb_header(rdev, &op->header);
    if (err != 0)
        return err;

    return stream_flush(rdev->raw_sb_stream);
}

/**
 * Check a checksummed stream's checksum against an expected value.
 *
 * @param[in]   checksum_stream     The stream to check
 * @param[in]   size                The size to read
 * @param[in]   expected_checksum   The expected checksum
 *
 * @return 0 if checksums match, a negative error code if read failed,
 *         -VRT_ERR_SB_CHECKSUM if the checksums don't match.
 */
static int __check_sb_checksum(stream_t *checksum_stream, uint64_t size,
                               checksum_t expected_checksum)
{
    int err;
    uint64_t remaining;
    checksum_t computed_checksum;

    err = stream_rewind(checksum_stream);
    if (err != 0)
        return err;

    remaining = size;

    while (remaining > 0)
    {
        int r, n;
        char buf[512]; /* We don't really mind the size of the buffer, the
                        * underlying blockdevice_stream caches. */

        n = MIN(sizeof(buf), remaining);

        /* We don't care about the read contents, we just read to let the
         * checksum stream do his job. You'll be informed as soon as possible
         */
        r = stream_read(checksum_stream, buf, n);
        if (r < 0)
            return err;
        else if (r != n)
            return -EIO;

        remaining -= r;
    }

    computed_checksum = checksum_stream_get_value(checksum_stream);
    if (computed_checksum != expected_checksum)
    {
        exalog_error("Invalid checksum "CHECKSUM_FMT", expected "CHECKSUM_FMT,
                     computed_checksum, expected_checksum);
        return -VRT_ERR_SB_CHECKSUM;
    }

    /* Rewind so that the stream is ready. This also has the
       effect of resetting the checksum stream's checksum. */
    return stream_rewind(checksum_stream);
}

int vrt_rdev_begin_superblock_read(vrt_realdev_t *rdev, uint64_t sb_version,
                                   superblock_read_op_t *op)
{
    superblock_header_t headers[2];
    int i;
    int err;

    err = __read_sb_headers(rdev, headers);
    if (err != 0)
        return err;

    for (i = 0; i < 2; i++)
    {
        if (__check_sb_header(&headers[i], i) != 0)
            continue;

        if (__check_sb_checksum(rdev->checksum_sb_streams[i], headers[i].data_size,
                                headers[i].checksum) != 0)
            continue;

        if (headers[i].sb_version == sb_version)
            break;
    }

    if (i == 2)
        return -VRT_ERR_SB_NOT_FOUND;

    op->header = headers[i];
    op->stream = rdev->sb_data_streams[op->header.position];

    /* Rewind so that the stream is ready for reading. */
    return stream_rewind(op->stream);
}

int vrt_rdev_end_superblock_read(vrt_realdev_t *rdev, superblock_read_op_t *op)
{
    uint64_t sb_data_size = stream_tell(op->stream);

    if (op->header.data_size != sb_data_size)
    {
        exalog_error("SB data size (%"PRIu64") mismatch header (%"PRIu64")",
                     sb_data_size, op->header.data_size);

        return -VRT_ERR_SB_CORRUPTION;
    }

    return 0;
}
