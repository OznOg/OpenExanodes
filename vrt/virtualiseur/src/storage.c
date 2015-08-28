/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/include/storage.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

storage_t *storage_alloc(void)
{
    storage_t *storage;

    storage = os_malloc(sizeof(storage_t));
    if (storage == NULL)
        return NULL;

    storage->spof_groups = NULL;
    storage->num_spof_groups = 0;
    storage->chunk_size = 0;

    return storage;
}

void __storage_free(storage_t *storage)
{
    if (storage == NULL)
        return;

    /* XXX Should we free the spof groups themselves? (the devices
           they contain, etc) */

    os_free(storage->spof_groups);

    storage->num_spof_groups = 0;
    storage->chunk_size = 0;

    os_free(storage);
}

/**
 * Set the storage chunk size. This must only be done once (and never change).
 *
 * @param[in,out] storage      Storage in which we set the size
 * @param[in]     chunk_size   Size of chunks, in Kbytes
 *
 * @return 0 if successful, -EINVAL if the chunk size is invalid
 */
static int storage_set_chunk_size(storage_t *storage, uint32_t chunk_size)
{
    EXA_ASSERT(storage);

    /* We don't allow changing the storage's chunk size once it's been set */
    EXA_ASSERT(storage->chunk_size == 0 || storage->chunk_size == chunk_size);

    if (chunk_size == 0)
        return -EINVAL;

    storage->chunk_size = chunk_size;

    return 0;
}

int storage_add_spof_group(storage_t *storage, spof_id_t spof_id,
                                  spof_group_t **sg)
{
    uint32_t i;
    spof_group_t *new_spof_groups;

    EXA_ASSERT(storage != NULL);

    EXA_ASSERT(sg != NULL);

    *sg = NULL;

    if (!SPOF_ID_IS_VALID(spof_id))
        return -EINVAL;

    for (i = 0; i < storage->num_spof_groups; i++)
        if (storage->spof_groups[i].spof_id == spof_id)
            return -EEXIST;

    if (storage->num_spof_groups == EXA_MAX_NODES_NUMBER)
        return -ENOSPC;

    new_spof_groups = os_realloc(storage->spof_groups,
            (storage->num_spof_groups + 1) * sizeof(spof_group_t));

    if (new_spof_groups == NULL)
        return -ENOMEM;

    spof_group_init(&new_spof_groups[storage->num_spof_groups]);
    new_spof_groups[storage->num_spof_groups].spof_id = spof_id;

    *sg = &new_spof_groups[storage->num_spof_groups];

    storage->spof_groups = new_spof_groups;
    storage->num_spof_groups++;

    return 0;
}

spof_group_t *storage_get_spof_group_by_id(const storage_t *storage,
                                           spof_id_t spof_id)
{
    uint32_t i;

    for (i = 0; i < storage->num_spof_groups; i++)
        if (storage->spof_groups[i].spof_id == spof_id)
            return &storage->spof_groups[i];

    return NULL;
}

uint64_t storage_get_spof_group_total_chunk_count(const storage_t *storage,
                                                  spof_id_t spof_id)
{
    spof_group_t *sg;

    EXA_ASSERT(storage != NULL);
    EXA_ASSERT(SPOF_ID_IS_VALID(spof_id));

    sg = storage_get_spof_group_by_id(storage, spof_id);
    EXA_ASSERT(sg != NULL);

    return spof_group_total_chunk_count(sg);
}

uint64_t storage_get_spof_group_free_chunk_count(const storage_t *storage,
                                                 spof_id_t spof_id)
{
    spof_group_t *sg;

    EXA_ASSERT(storage != NULL);
    EXA_ASSERT(SPOF_ID_IS_VALID(spof_id));

    sg = storage_get_spof_group_by_id(storage, spof_id);
    EXA_ASSERT(sg != NULL);

    return spof_group_free_chunk_count(sg);
}

void storage_spof_iterator_begin(storage_spof_iter_t *iter, const storage_t *storage)
{
    EXA_ASSERT(iter != NULL);
    EXA_ASSERT(storage != NULL);

    iter->storage = storage;
    iter->index = 0;
}

void storage_spof_iterator_end(storage_spof_iter_t *iter)
{
    EXA_ASSERT(iter != NULL);
    EXA_ASSERT(iter->storage != NULL);

    iter->storage = NULL;
}

spof_id_t storage_spof_iterator_get(storage_spof_iter_t *iter)
{
    const spof_group_t *spof_group;

    EXA_ASSERT(iter != NULL);
    EXA_ASSERT(iter->storage != NULL);

    if (iter->index >= iter->storage->num_spof_groups)
        return SPOF_ID_NONE;

    spof_group = &iter->storage->spof_groups[iter->index];
    iter->index++;

    return spof_group->spof_id;
}

void storage_rdev_iterator_begin(storage_rdev_iter_t *iter, const storage_t *storage)
{
    EXA_ASSERT(iter != NULL);
    EXA_ASSERT(storage != NULL);

    iter->storage = storage;
    iter->spof_group_index = 0;
    iter->realdev_index = 0;
}

void storage_rdev_iterator_end(storage_rdev_iter_t *iter)
{
    EXA_ASSERT(iter != NULL);
    EXA_ASSERT(iter->storage != NULL);

    iter->storage = NULL;
}

vrt_realdev_t *storage_rdev_iterator_get(storage_rdev_iter_t *iter)
{
    const spof_group_t *spof_group;
    vrt_realdev_t *rdev = NULL;

    EXA_ASSERT(iter != NULL);
    EXA_ASSERT(iter->storage != NULL);

    if (iter->spof_group_index >= iter->storage->num_spof_groups)
        return NULL;

    spof_group = &iter->storage->spof_groups[iter->spof_group_index];

    if (iter->realdev_index >= spof_group->nb_realdevs)
    {
        iter->realdev_index = 0;
        iter->spof_group_index++;
        return storage_rdev_iterator_get(iter);
    }

    rdev = spof_group->realdevs[iter->realdev_index];
    iter->realdev_index++;

    return rdev;
}

int storage_get_num_realdevs(const storage_t *storage)
{
    int i;
    int count = 0;

    for (i = 0; i < storage->num_spof_groups; i++)
        count += storage->spof_groups[i].nb_realdevs;

    return count;
}

int storage_add_rdev(storage_t *storage, spof_id_t spof_id, vrt_realdev_t *rdev)
{
    int err;
    spof_group_t *sg;

    EXA_ASSERT(storage != NULL);

    if (!SPOF_ID_IS_VALID(spof_id) || rdev == NULL)
        return -EINVAL;

    if (storage_get_num_realdevs(storage) == NBMAX_DISKS_PER_GROUP)
        return -ENOSPC;

    sg = storage_get_spof_group_by_id(storage,  spof_id);
    if (sg == NULL)
    {
        err = storage_add_spof_group(storage, spof_id, &sg);
        if (err != 0)
            return err;
    }

    return spof_group_add_rdev(sg, rdev);
}

int storage_del_rdev(storage_t *storage, spof_id_t spof_id, vrt_realdev_t *rdev)
{
    uint32_t i;

    EXA_ASSERT(storage != NULL);

    if (!SPOF_ID_IS_VALID(spof_id) || rdev == NULL)
        return -EINVAL;

    for (i = 0; i < storage->num_spof_groups; i++)
    {
        spof_group_t *sg = &storage->spof_groups[i];

        if (sg->spof_id == spof_id)
            return spof_group_del_rdev(sg, rdev);
    }

    return -ENOENT;
}

vrt_realdev_t *storage_get_rdev(const storage_t *storage, const exa_uuid_t *uuid)
{
    struct vrt_realdev *rdev;
    storage_rdev_iter_t iter;

    storage_rdev_iterator_begin(&iter, storage);

    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
        if (uuid_is_equal(&rdev->uuid, uuid))
            break;

    storage_rdev_iterator_end(&iter);

    return rdev;
}

int storage_rdev_header_read(storage_rdev_header_t *header, stream_t *stream)
{
    int r;

    r = stream_read(stream, header, sizeof(storage_rdev_header_t));
    if (r < 0)
        return r;
    else if (r < sizeof(storage_rdev_header_t))
        return -EIO;

    return 0;
}

uint64_t storage_rdev_serialized_size(const vrt_realdev_t *rdev)
{
    return sizeof(storage_rdev_header_t);
}

int storage_rdev_serialize(const vrt_realdev_t *rdev, stream_t *stream)
{
    storage_rdev_header_t header;
    int w;

    header.uuid = rdev->uuid;
    header.total_chunks_count = rdev->chunks.total_chunks_count;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    return 0;
}

int storage_rdev_deserialize(vrt_realdev_t **rdev, storage_t *storage,
                                stream_t *stream)
{
    storage_rdev_header_t header;
    int err;

    err = storage_rdev_header_read(&header, stream);
    if (err != 0)
        return err;

    *rdev = storage_get_rdev(storage, &header.uuid);
    if (*rdev == NULL)
        return -VRT_ERR_SB_CORRUPTION;

    storage_initialize_rdev_chunks_info(storage, *rdev, header.total_chunks_count);

    return 0;
}

int storage_header_read(storage_header_t *header, stream_t *stream)
{
    int r;

    r = stream_read(stream, header, sizeof(storage_header_t));
    if (r < 0)
        return r;
    else if (r < sizeof(storage_header_t))
        return -EIO;

    return 0;
}

uint64_t storage_serialized_size(const storage_t *storage)
{
    uint64_t size;
    const struct vrt_realdev *rdev;
    storage_rdev_iter_t iter;

    size = sizeof(storage_header_t);

    storage_rdev_iterator_begin(&iter, storage);

    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
	size += storage_rdev_serialized_size(rdev);

    storage_rdev_iterator_end(&iter);

    return size;
}

/**
 * Serialize a storage.
 *
 * @param[in]   The storage to serialize
 * @param[in]   The stream to serialize to
 *
 * @return 0 if successful, a negative error code otherwise
 *
 * @attention This serializes only the chunk_size and minimal
 * rdev information
 */
int storage_serialize(const storage_t *storage, stream_t *stream)
{
    storage_header_t header;
    int w, err;
    const struct vrt_realdev *rdev;
    storage_rdev_iter_t iter;

    header.magic = STORAGE_HEADER_MAGIC;
    header.format = STORAGE_HEADER_FORMAT;
    header.chunk_size = storage->chunk_size;
    header.nb_rdevs = storage_get_num_realdevs(storage);

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    storage_rdev_iterator_begin(&iter, storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
	err = storage_rdev_serialize(rdev, stream);
        if (err != 0)
            return err;
    }
    storage_rdev_iterator_end(&iter);

    return 0;
}

/**
 * Deserialize a storage.
 *
 * @param[in]   The storage to deserialize
 * @param[in]   The stream to serialize from
 *
 * @return 0 if successful, a negative error code otherwise
 *
 * @attention This deserializes only the chunk_size and minimal
 * rdev information, the storage is expected to already contain
 * SPOF groups and realdevs.
 */
int storage_deserialize(storage_t *storage, stream_t *stream)
{
    storage_header_t header;
    int err, i;

    err = storage_header_read(&header, stream);
    if (err != 0)
        return err;

    if (header.magic != STORAGE_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    if (header.format != STORAGE_HEADER_FORMAT)
        return -VRT_ERR_SB_FORMAT;

    if (header.nb_rdevs != storage_get_num_realdevs(storage))
        return -VRT_ERR_SB_CORRUPTION;

    storage_set_chunk_size(storage, header.chunk_size);

    for (i = 0; i < header.nb_rdevs; i++)
    {
        vrt_realdev_t *rdev;

        err = storage_rdev_deserialize(&rdev, storage, stream);
        if (err != 0)
            return err;

        EXA_ASSERT(rdev != NULL);
    }

    return 0;
}

/* FIXME: Just the serialized parts are tested for equality.
 * Should we test more?
 */
bool storage_equals(const storage_t *s1, const storage_t *s2)
{
    const vrt_realdev_t *r1;
    storage_rdev_iter_t iter;

    if (s1->chunk_size != s2->chunk_size)
        return false;

    if (storage_get_num_realdevs(s1) != storage_get_num_realdevs(s2))
        return false;

    storage_rdev_iterator_begin(&iter, s1);

    while ((r1 = storage_rdev_iterator_get(&iter)) != NULL)
    {
        vrt_realdev_t *r2 = storage_get_rdev(s2, &r1->uuid);
        if (r2 == NULL)
            return false;
        if (r1->chunks.total_chunks_count != r2->chunks.total_chunks_count)
            return false;
    }

    storage_rdev_iterator_end(&iter);

    return true;
}

void storage_initialize_rdev_chunks_info(storage_t *storage, vrt_realdev_t *rdev,
                                        uint64_t total_chunks_count)
{
    int i;

    rdev->chunks.chunk_size = KBYTES_2_SECTORS(storage->chunk_size);
    rdev->chunks.total_chunks_count = total_chunks_count;
    rdev->chunks.free_chunks_count = total_chunks_count;

    for (i = 0; i < total_chunks_count; i++)
        rdev->chunks.free_chunks = extent_list_add_value(rdev->chunks.free_chunks, i);
}

/**
 * Cut an rdev into chunks
 *
 * @param[in] storage       The storage
 * @param[in] rdev          The rdev to cut in chunks
 *
 * @return 0 if successful, a negative error code otherwise
 */
int storage_cut_rdev_in_chunks(storage_t *storage, vrt_realdev_t *rdev)
{
    uint64_t total_size = vrt_realdev_get_usable_size(rdev);
    uint32_t chunk_size = KBYTES_2_SECTORS(storage->chunk_size);

    EXA_ASSERT(total_size > 0);
    EXA_ASSERT(chunk_size > 0);

    if (chunk_size > total_size)
        return -VRT_ERR_RDEV_TOO_SMALL;

    storage_initialize_rdev_chunks_info(storage, rdev, total_size / chunk_size);

    return EXA_SUCCESS;
}


int storage_cut_in_chunks(storage_t *storage, uint32_t chunk_size)
{
    int err;
    storage_rdev_iter_t iter;
    vrt_realdev_t *rdev;
    uint64_t total_chunks_count = 0;

    err = storage_set_chunk_size(storage, chunk_size);
    if (err)
        return err;

    storage_rdev_iterator_begin(&iter, storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        err = storage_cut_rdev_in_chunks(storage, rdev);
        if (err != 0)
            break;
        total_chunks_count += rdev->chunks.total_chunks_count;
    }
    storage_rdev_iterator_end(&iter);

    if (err == 0 && total_chunks_count > VRT_NBMAX_CHUNKS_PER_GROUP)
        return -VRT_ERR_TOO_MANY_CHUNKS;

    return err;
}
