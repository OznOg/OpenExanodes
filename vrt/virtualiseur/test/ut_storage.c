/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/virtualiseur/include/storage.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/fake_rdev.h"
#include "vrt/common/include/memory_stream.h"
#include "os/include/os_random.h"
#include "common/include/uuid.h"

UT_SECTION(storage_init)

ut_test(init_with_valid_values_succeeds)
{
    storage_t *sto = storage_alloc();
    UT_ASSERT(sto != NULL);
    storage_free(sto);
}

UT_SECTION(storage_add_spof_group)

ut_test(adding_SPOF_ID_NONE_returns_EINVAL)
{
    storage_t *sto = storage_alloc();
    spof_group_t *sg;

    UT_ASSERT_EQUAL(-EINVAL, storage_add_spof_group(sto, SPOF_ID_NONE, &sg));
    UT_ASSERT(sg == NULL);

    storage_free(sto);
}

ut_test(adding_valid_spof_returns_zero)
{
    storage_t *sto = storage_alloc();
    spof_id_t id;
    spof_group_t *sg;


    /* Add "arbitrary" spof ids (multiples of 5) */
    for (id = 1; id <= EXA_MAX_NODES_NUMBER; id++)
    {
        UT_ASSERT_EQUAL(0, storage_add_spof_group(sto, id * 5, &sg));
        UT_ASSERT(sg != NULL);
        UT_ASSERT_EQUAL(id * 5, sg->spof_id);
    }
    storage_free(sto);
}

ut_test(adding_too_much_spofs_returns_ENOSPC)
{
    storage_t *sto = storage_alloc();
    spof_id_t id;
    spof_group_t *sg;

    /* Add "arbitrary" spof ids (multiples of 3) */
    for (id = 1; id <= EXA_MAX_NODES_NUMBER; id++)
        UT_ASSERT_EQUAL(0, storage_add_spof_group(sto, id * 3, &sg));

    UT_ASSERT_EQUAL(-ENOSPC, storage_add_spof_group(sto, 7, &sg));
    UT_ASSERT(sg == NULL);
    storage_free(sto);
}

ut_test(adding_same_spof_twice_returns_EEXIST)
{
    storage_t *sto = storage_alloc();
    spof_group_t *sg;

    UT_ASSERT_EQUAL(0, storage_add_spof_group(sto, 3, &sg));
    UT_ASSERT(sg != NULL);

    UT_ASSERT_EQUAL(-EEXIST, storage_add_spof_group(sto, 3, &sg));
    UT_ASSERT(sg == NULL);

    storage_free(sto);
}

UT_SECTION(storage_add_rdev)

ut_test(adding_rdev_to_invalid_spof_returns_EINVAL)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdev;

    UT_ASSERT_EQUAL(-EINVAL, storage_add_rdev(sto, SPOF_ID_NONE, &rdev));
    storage_free(sto);
}

ut_test(adding_rdev_to_correct_spofs_returns_zero)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdevs[5];

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 1, &rdevs[0]));
    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 1, &rdevs[1]));

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 2, &rdevs[2]));

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 3, &rdevs[3]));
    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 3, &rdevs[4]));
    storage_free(sto);
}

ut_test(adding_rdev_adds_corresponding_spof)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdevs[5];

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 1, &rdevs[0]));
    UT_ASSERT_EQUAL(1, sto->num_spof_groups);
    UT_ASSERT_EQUAL(1, sto->spof_groups[0].spof_id);

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 2, &rdevs[2]));
    UT_ASSERT_EQUAL(2, sto->num_spof_groups);
    UT_ASSERT_EQUAL(1, sto->spof_groups[0].spof_id);
    UT_ASSERT_EQUAL(2, sto->spof_groups[1].spof_id);


    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 3, &rdevs[3]));
    UT_ASSERT_EQUAL(3, sto->num_spof_groups);
    UT_ASSERT_EQUAL(1, sto->spof_groups[0].spof_id);
    UT_ASSERT_EQUAL(2, sto->spof_groups[1].spof_id);
    UT_ASSERT_EQUAL(3, sto->spof_groups[2].spof_id);

    storage_free(sto);
}

UT_SECTION(storage_del_rdev)

ut_test(removing_rdev_from_invalid_spof_returns_EINVAL)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdev;

    UT_ASSERT_EQUAL(-EINVAL, storage_del_rdev(sto, SPOF_ID_NONE, &rdev));
    storage_free(sto);
}

ut_test(removing_rdev_from_storage_with_no_spof_groups_returns_ENOENT)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdev;

    UT_ASSERT_EQUAL(-ENOENT, storage_del_rdev(sto, 3, &rdev));
    storage_free(sto);
}

ut_test(removing_rdev_from_unknown_spof_returns_ENOENT)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdev;

    UT_ASSERT_EQUAL(-ENOENT, storage_del_rdev(sto, 5, &rdev));
    storage_free(sto);
}

ut_test(removing_unknown_rdev_from_existing_spofs_returns_ENOENT)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdev;
    spof_group_t *sg;

    storage_add_spof_group(sto, 1, &sg);

    UT_ASSERT_EQUAL(-ENOENT, storage_del_rdev(sto, 1, &rdev));
    storage_free(sto);
}

ut_test(removing_rdev_from_existing_spofs_returns_0)
{
    storage_t *sto = storage_alloc();
    vrt_realdev_t rdev;

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 1, &rdev));
    UT_ASSERT_EQUAL(0, storage_del_rdev(sto, 1, &rdev));
    storage_free(sto);
}

UT_SECTION(storage_spof_iter)

ut_test(iterating_over_empty_storage_yields_SPOF_ID_NONE)
{
    storage_t *sto = storage_alloc();
    storage_spof_iter_t iter;

    storage_spof_iterator_begin(&iter, sto);
    UT_ASSERT_EQUAL(SPOF_ID_NONE, storage_spof_iterator_get(&iter));
    storage_spof_iterator_end(&iter);
    storage_free(sto);
}

static int __cmp_spof_ids(const void *a, const void *b)
{
    return *(const spof_id_t *)a - *(const spof_id_t *)b;
}

ut_test(iterating_over_non_empty_storage_yields_all_spof_ids)
{
    spof_id_t spof_ids_1[3] = { 5, 883, 60 };
    spof_id_t spof_ids_2[3];
    storage_t *sto = storage_alloc();
    storage_spof_iter_t iter;
    spof_id_t spof_id;
    unsigned count;
    spof_group_t *sg;

    storage_add_spof_group(sto, 5, &sg);
    storage_add_spof_group(sto, 883, &sg);
    storage_add_spof_group(sto, 60, &sg);

    count = 0;
    storage_spof_iterator_begin(&iter, sto);
    while ((spof_id = storage_spof_iterator_get(&iter)) != SPOF_ID_NONE)
        spof_ids_2[count++] = spof_id;
    storage_spof_iterator_end(&iter);

    qsort(spof_ids_1, 3, sizeof(spof_id_t), __cmp_spof_ids);
    qsort(spof_ids_2, 3, sizeof(spof_id_t), __cmp_spof_ids);

    UT_ASSERT_EQUAL(0, memcmp(spof_ids_1, spof_ids_2, sizeof(spof_ids_1)));
    UT_ASSERT_EQUAL(3, count);
    storage_free(sto);
}

UT_SECTION(storage_rdev_iter)

ut_test(iterating_rdevs_over_empty_storage_yields_NULL)
{
    storage_t *sto = storage_alloc();
    storage_rdev_iter_t iter;

    storage_rdev_iterator_begin(&iter, sto);
    UT_ASSERT(storage_rdev_iterator_get(&iter) == NULL);
    storage_rdev_iterator_end(&iter);
    storage_free(sto);
}

ut_test(iterating_rdevs_over_non_empty_storage_yields_all_rdevs)
{
    vrt_realdev_t rdevs[11];
    vrt_realdev_t *cur_rdev;
    storage_t *sto = storage_alloc();
    storage_rdev_iter_t iter;
    unsigned count;

    storage_add_rdev(sto, 5, &rdevs[0]);
    storage_add_rdev(sto, 5, &rdevs[1]);
    storage_add_rdev(sto, 5, &rdevs[2]);
    storage_add_rdev(sto, 5, &rdevs[3]);

    storage_add_rdev(sto, 883, &rdevs[4]);
    storage_add_rdev(sto, 883, &rdevs[5]);
    storage_add_rdev(sto, 883, &rdevs[6]);

    storage_add_rdev(sto, 60, &rdevs[7]);
    storage_add_rdev(sto, 60, &rdevs[8]);
    storage_add_rdev(sto, 60, &rdevs[9]);
    storage_add_rdev(sto, 60, &rdevs[10]);

    count = 0;
    storage_rdev_iterator_begin(&iter, sto);
    while ((cur_rdev = storage_rdev_iterator_get(&iter)) != NULL)
        UT_ASSERT(cur_rdev == &rdevs[count++]);

    storage_rdev_iterator_end(&iter);
    UT_ASSERT_EQUAL(11, count);
    storage_free(sto);
}

ut_test(storage_serialize_deserialize_is_identity)
{
    char buf[1024];
    storage_t *sto = storage_alloc();
    storage_t *sto2 = storage_alloc();
    vrt_realdev_t *rdevs[2];
    stream_t *stream;
    exa_uuid_t rdev_uuid, nbd_uuid;
    uint64_t rdev_chunk_size[2], rdev_total_chunks_count[2];

    os_random_init();

    sto->chunk_size = 262144;

    UT_ASSERT_EQUAL(-EINVAL, storage_serialize(sto, NULL));

    uuid_generate(&rdev_uuid);
    uuid_generate(&nbd_uuid);
    rdevs[0] = make_fake_rdev(0, 1, &rdev_uuid, &nbd_uuid, 12 * 1024 * 1024,
                              true, true);
    uuid_generate(&rdev_uuid);
    uuid_generate(&nbd_uuid);
    rdevs[1] = make_fake_rdev(0, 2, &rdev_uuid, &nbd_uuid, 12 * 1024 * 1024,
                              true, true);

    storage_add_rdev(sto, 1, rdevs[0]);
    storage_add_rdev(sto, 2, rdevs[1]);
    storage_add_rdev(sto2, 1, rdevs[0]);
    storage_add_rdev(sto2, 2, rdevs[1]);

    storage_cut_in_chunks(sto, sto->chunk_size);

    memory_stream_open(&stream, buf, sizeof(buf), STREAM_ACCESS_RW);
    UT_ASSERT_EQUAL(0, storage_serialize(sto, stream));
    stream_rewind(stream);
    UT_ASSERT_EQUAL(0, storage_deserialize(sto2, stream));
    UT_ASSERT(storage_equals(sto, sto2));

    /* Reset the rdevs chunks count, and check they're correctly
     * rebuilt from deserialization */
    rdev_chunk_size[0] = rdevs[0]->chunks.chunk_size;
    rdev_chunk_size[1] = rdevs[1]->chunks.chunk_size;
    rdev_total_chunks_count[0] = rdevs[0]->chunks.total_chunks_count;
    rdev_total_chunks_count[1] = rdevs[1]->chunks.total_chunks_count;

    rdevs[0]->chunks.chunk_size = 0;
    rdevs[1]->chunks.chunk_size = 0;
    rdevs[0]->chunks.total_chunks_count = 0;
    rdevs[1]->chunks.total_chunks_count = 0;

    stream_rewind(stream);
    UT_ASSERT_EQUAL(0, storage_deserialize(sto2, stream));
    UT_ASSERT(storage_equals(sto, sto2));

    UT_ASSERT_EQUAL(rdev_chunk_size[0], rdevs[0]->chunks.chunk_size);
    UT_ASSERT_EQUAL(rdev_chunk_size[1], rdevs[1]->chunks.chunk_size);
    UT_ASSERT_EQUAL(rdev_total_chunks_count[0], rdevs[0]->chunks.total_chunks_count);
    UT_ASSERT_EQUAL(rdev_total_chunks_count[1], rdevs[1]->chunks.total_chunks_count);

    stream_close(stream);

    storage_free(sto);
    storage_free(sto2);

    os_random_cleanup();
}
