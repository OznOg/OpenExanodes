/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_layout.h"

/* Fake of VRT */
#include "vrt/virtualiseur/fakes/fake_rdev.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/empty_perf_definitions.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/empty_request_definitions.h"
#include "vrt/virtualiseur/fakes/empty_nodes_definitions.h"
#include "vrt/virtualiseur/fakes/empty_vrt_threads_definitions.h"

/* Fake other exanodes components */
#include "vrt/virtualiseur/fakes/empty_nbd_client_definitions.h"
#include "vrt/virtualiseur/fakes/empty_monitoring_definitions.h"
#include "vrt/virtualiseur/fakes/empty_msg_definitions.h"
#include "vrt/virtualiseur/fakes/empty_cmd_definitions.h"

#include "vrt/layout/sstriping/include/sstriping.h"
#include "vrt/layout/rain1/include/rain1.h"

#include "vrt/common/include/memory_stream.h"
#include "vrt/common/include/stat_stream.h"

#include "common/include/exa_error.h"

#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_random.h"

#define RDEV_SIZE        ((uint64_t)1024*1024*1024)    /* 512 GB in sectors */

#define NUM_SPOF_GROUPS  7

static struct vrt_realdev *rdevs[NBMAX_DISKS_PER_GROUP];
static struct vrt_realdev *rdevs2[NBMAX_DISKS_PER_GROUP];
static storage_t *sto = NULL;
static storage_t *sto2 = NULL;

static uint32_t chunk_size = 262144; /* Default value, in Kbytes */

static char *__buf = NULL;
static stream_t *memory_stream;
static stream_t *stream;
static stream_stats_t stats;

UT_SECTION(serialization)

ut_setup()
{
    int i;

    os_random_init();

    for (i = 0; i < NUM_SPOF_GROUPS; i++)
    {
        exa_uuid_t rdev_uuid, nbd_uuid;
        spof_id_t spof_id = i + 1;

        uuid_generate(&rdev_uuid);
        uuid_generate(&nbd_uuid);

        rdevs[i] = make_fake_rdev(i, spof_id, &rdev_uuid, &nbd_uuid, RDEV_SIZE, true, true);
        rdevs[i]->index = i;

        UT_ASSERT(rdevs[i] != NULL);

        rdevs2[i] = make_fake_rdev(i, spof_id, &rdev_uuid, &nbd_uuid, RDEV_SIZE, true, true);
        rdevs2[i]->index = i;

        UT_ASSERT(rdevs2[i] != NULL);
    }

    sto = make_fake_storage(NUM_SPOF_GROUPS, chunk_size, rdevs, NUM_SPOF_GROUPS);
    UT_ASSERT(sto != NULL);

    sto2 = make_fake_storage(NUM_SPOF_GROUPS, chunk_size, rdevs2, NUM_SPOF_GROUPS);
    UT_ASSERT(sto2 != NULL);

    sstriping_init();
    rain1_init(0, 0);

    __buf = os_malloc(SECTORS_TO_BYTES(VRT_SB_AREA_SIZE));
    UT_ASSERT_EQUAL(0, memory_stream_open(&memory_stream, __buf,
                                          SECTORS_TO_BYTES(VRT_SB_AREA_SIZE),
                                          STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, stat_stream_open(&stream, memory_stream, &stats));
}

ut_cleanup()
{
    int i;

    stream_close(stream);
    stream_close(memory_stream);

    os_free(__buf);

    rain1_cleanup();
    sstriping_cleanup();

    storage_free(sto);
    storage_free(sto2);
    for (i = 0; i < NUM_SPOF_GROUPS; i++)
    {
        os_free(rdevs[i]);
        os_free(rdevs2[i]);
    }

    os_random_cleanup();
}

ut_test(serialize_deserialize_sstriping_group_is_identity)
{
    exa_uuid_t uuid, vol1_uuid, vol2_uuid;
    vrt_group_t *group, *group2;
    vrt_volume_t *vol1, *vol2;
    uint64_t computed_size, actual_size;
    int ret;
    char msg[EXA_MAXSIZE_LINE + 1];

    uuid_generate(&uuid);
    group = vrt_group_alloc("group", &uuid, vrt_get_layout("sstriping"));
    UT_ASSERT(group != NULL);

    group->status = EXA_GROUP_OK;
    group->storage = sto;
    ret = group->layout->group_create(group->storage,
                                      &group->layout_data,
                                      NUM_SPOF_GROUPS,
                                      KBYTES_2_SECTORS(chunk_size),
				      1024,
                                      32768,
                                      0,
				      0,
                                      msg);
    UT_ASSERT_EQUAL(0, ret);

    uuid_generate(&vol1_uuid);
    UT_ASSERT_EQUAL(0,
            vrt_group_create_volume(group, &vol1, &vol1_uuid, "volume1", 10));

    uuid_generate(&vol2_uuid);
    UT_ASSERT_EQUAL(0,
            vrt_group_create_volume(group, &vol2, &vol2_uuid, "volume2", 10));

    UT_ASSERT_EQUAL(0, vrt_group_serialize(group, stream));
    ut_printf("Wrote %"PRIu64" bytes.", stream_tell(stream));

    computed_size = vrt_group_serialized_size(group);
    actual_size = stats.write_stats.total_bytes;
    UT_ASSERT_EQUAL(actual_size, computed_size);

    stream_seek(stream, 0, STREAM_SEEK_FROM_BEGINNING);
    UT_ASSERT_EQUAL(0, vrt_group_deserialize(&group2, sto2, stream, &group->uuid));

    UT_ASSERT(vrt_group_equals(group, group2));

    /* Don't let ut_cleanup clean them (will be done by group_free) */
    sto = NULL;
    sto2 = NULL;

    vrt_group_free(group);
    vrt_group_free(group2);
}

ut_test(deserialize_failures)
{
    exa_uuid_t uuid, vol1_uuid, vol2_uuid;
    vrt_group_t *group, *group2;
    vrt_volume_t *vol1, *vol2;
    int ret;
    char msg[EXA_MAXSIZE_LINE + 1];

    uuid_generate(&uuid);
    group = vrt_group_alloc("group", &uuid, vrt_get_layout("sstriping"));
    UT_ASSERT(group != NULL);

    group->status = EXA_GROUP_OK;
    group->storage = sto;
    ret = group->layout->group_create(group->storage,
                                      &group->layout_data,
                                      NUM_SPOF_GROUPS,
                                      KBYTES_2_SECTORS(chunk_size),
				      1024,
                                      32768,
                                      0,
				      0,
                                      msg);
    UT_ASSERT_EQUAL(0, ret);

    uuid_generate(&vol1_uuid);
    UT_ASSERT_EQUAL(0,
            vrt_group_create_volume(group, &vol1, &vol1_uuid, "volume1", 10));

    uuid_generate(&vol2_uuid);
    UT_ASSERT_EQUAL(0,
            vrt_group_create_volume(group, &vol2, &vol2_uuid, "volume2", 10));

    UT_ASSERT_EQUAL(0, vrt_group_serialize(group, stream));
    ut_printf("Wrote %"PRIu64" bytes.", stream_tell(stream));

    stream_rewind(stream);
    UT_ASSERT_EQUAL(-VRT_ERR_SB_UUID_MISMATCH,
                    vrt_group_deserialize(&group2, sto, stream, &exa_uuid_zero));

    os_get_random_bytes(__buf, 16);
    stream_rewind(stream);
    UT_ASSERT_EQUAL(-VRT_ERR_SB_MAGIC,
                    vrt_group_deserialize(&group2, sto2, stream, &group->uuid));

    UT_ASSERT(group2 == NULL);

    /* Don't let ut_cleanup clean sto (will be done by group_free) */
    sto = NULL;

    vrt_group_free(group);
}

ut_test(serialize_deserialize_rainX_group_is_identity)
{
    exa_uuid_t uuid, vol1_uuid, vol2_uuid;
    vrt_group_t *group, *group2;
    vrt_volume_t *vol1, *vol2;
    uint64_t computed_size, actual_size;
    int ret;
    char msg[EXA_MAXSIZE_LINE + 1];

    uuid_generate(&uuid);
    group = vrt_group_alloc("group", &uuid, vrt_get_layout("rain1"));
    UT_ASSERT(group != NULL);

    group->status = EXA_GROUP_OK;
    group->storage = sto;
    ret = group->layout->group_create(group->storage,
                                      &group->layout_data,
                                      7, /* slot width */
                                      KBYTES_2_SECTORS(chunk_size),
				      1024,
                                      32768,
                                      0,
				      2, /* spares */
                                      msg);
    UT_ASSERT_EQUAL(0, ret);

    uuid_generate(&vol1_uuid);
    UT_ASSERT_EQUAL(0,
            vrt_group_create_volume(group, &vol1, &vol1_uuid, "volume1", 10));

    uuid_generate(&vol2_uuid);
    UT_ASSERT_EQUAL(0,
            vrt_group_create_volume(group, &vol2, &vol2_uuid, "volume2", 10));

    UT_ASSERT_EQUAL(0, vrt_group_serialize(group, stream));
    ut_printf("Wrote %"PRIu64" bytes.", stream_tell(stream));

    computed_size = vrt_group_serialized_size(group);
    actual_size = stats.write_stats.total_bytes;
    UT_ASSERT_EQUAL(actual_size, computed_size);

    stream_seek(stream, 0, STREAM_SEEK_FROM_BEGINNING);
    UT_ASSERT_EQUAL(0, vrt_group_deserialize(&group2, sto2, stream, &group->uuid));

    UT_ASSERT(vrt_group_equals(group, group2));

    /* Don't let ut_cleanup clean them (will be done by group_free) */
    sto = NULL;
    sto2 = NULL;

    vrt_group_free(group);
    vrt_group_free(group2);
}
