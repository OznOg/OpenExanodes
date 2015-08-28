/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/virtualiseur/include/vrt_volume.h"

#include "vrt/virtualiseur/fakes/fake_rdev.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/fake_assembly_group.h"
#include "vrt/virtualiseur/fakes/empty_group_definitions.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/empty_request_definitions.h"
#include "vrt/virtualiseur/fakes/empty_module_definitions.h"
#include "vrt/virtualiseur/fakes/empty_nodes_definitions.h"

#include "vrt/common/include/memory_stream.h"
#include "vrt/common/include/stat_stream.h"

#include "os/include/os_mem.h"
#include "os/include/os_random.h"

#define RDEV_SIZE        (VRT_SB_AREA_SIZE * 5)

#define NUM_SPOF_GROUPS  3

static struct vrt_realdev *rdevs[NUM_SPOF_GROUPS];
static storage_t *sto = NULL;

static assembly_group_t *ag;

static char __buf[128];
static stream_t *memory_stream;
static stream_t *stream;
static stream_stats_t stats;

ut_setup()
{
    const uint32_t slot_width = NUM_SPOF_GROUPS;
    const uint32_t chunk_size = 262144 / 512; /* Default value, in sectors */
    int i;
    int err;

    os_random_init();

    for (i = 0; i < NUM_SPOF_GROUPS; i++)
    {
        exa_uuid_t rdev_uuid, nbd_uuid;
        spof_id_t spof_id = i + 1;

        uuid_generate(&rdev_uuid);
        uuid_generate(&nbd_uuid);

        rdevs[i] = make_fake_rdev(i, spof_id, &rdev_uuid, &nbd_uuid, RDEV_SIZE, true, true);
        UT_ASSERT(rdevs[i] != NULL);
    }

    sto = make_fake_storage(NUM_SPOF_GROUPS, chunk_size, rdevs, NUM_SPOF_GROUPS);
    UT_ASSERT(sto != NULL);

    /* Setup a valid assembly group (containing rdevs, etc) */
    ag = make_fake_ag(sto, slot_width);
    UT_ASSERT(ag != NULL);

    err = memory_stream_open(&memory_stream, __buf, sizeof(__buf), STREAM_ACCESS_RW);
    UT_ASSERT_EQUAL(0, err);

    err = stat_stream_open(&stream, memory_stream, &stats);
    UT_ASSERT_EQUAL(0, err);
}

ut_cleanup()
{
    int i;

    stream_close(stream);
    stream_close(memory_stream);

    assembly_group_cleanup(ag);
    os_free(ag);

    storage_free(sto);
    for (i = 0; i < NUM_SPOF_GROUPS; i++)
        os_free(rdevs[i]);

    os_random_cleanup();
}

ut_test(serialize_deserialize_is_identity)
{
    assembly_volume_t *av;
    exa_uuid_t uuid;
    vrt_volume_t *vol, *vol2;
    uint64_t computed_size, actual_size;

    uuid_generate(&uuid);
    UT_ASSERT_EQUAL(0, assembly_group_reserve_volume(ag, &uuid, 5, &av, sto));

    vol = vrt_volume_alloc(&uuid, "small", 4096);
    UT_ASSERT(vol != NULL);
    vol->assembly_volume = av;

    UT_ASSERT_EQUAL(0, vrt_volume_serialize(vol, stream));

    computed_size = vrt_volume_serialized_size(vol);
    actual_size = stats.write_stats.total_bytes;
    UT_ASSERT_EQUAL(actual_size, computed_size);

    stream_rewind(stream);
    UT_ASSERT_EQUAL(0, vrt_volume_deserialize(&vol2, ag, stream));

    UT_ASSERT(vrt_volume_equals(vol2, vol));

    vrt_volume_free(vol);
    vrt_volume_free(vol2);
}
