/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/layout/sstriping/src/lay_sstriping_group.h"
#include "vrt/layout/sstriping/src/lay_sstriping_superblock.h"

#include "vrt/virtualiseur/fakes/fake_rdev.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/fake_assembly_group.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/empty_group_definitions.h"

#include "vrt/common/include/memory_stream.h"
#include "vrt/common/include/stat_stream.h"

#include "os/include/os_mem.h"
#include "os/include/os_random.h"

/* FIXME For the fugly fakes below */
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "common/include/uuid.h"

#define RDEV_SIZE        (VRT_SB_AREA_SIZE * 5)

#define NUM_SPOF_GROUPS  3

static struct vrt_realdev *rdevs[NUM_SPOF_GROUPS] = { NULL, NULL, NULL };
static storage_t *sto = NULL;

static sstriping_group_t *ssg;

static char __buf[65536];
static stream_t *memory_stream;
static stream_t *stream;
static stream_stats_t stats;

/* XXX Probably badly leaking */
static sstriping_group_t *make_fake_ssg(storage_t *sto)
{
    const uint32_t slot_width = NUM_SPOF_GROUPS;
    assembly_group_t *ag = NULL;
    assembly_volume_t *av[3] = { NULL, NULL, NULL };
    sstriping_group_t *ssg = NULL;
    exa_uuid_t uuid;

    /* Setup a valid assembly group (containing rdevs, etc) */
    ag = make_fake_ag(sto, slot_width);
    if (ag == NULL)
        goto failed;

    ssg = os_malloc(sizeof(sstriping_group_t));
    if (ssg == NULL)
        goto failed;

    /* Reserve subspaces for what would be user volumes */
    uuid_generate(&uuid);
    if (assembly_group_reserve_volume(ag, &uuid, 5, &av[0], sto) != 0)
        goto failed;

    uuid_generate(&uuid);
    if (assembly_group_reserve_volume(ag, &uuid, 7, &av[1], sto) != 0)
        goto failed;

    uuid_generate(&uuid);
    if (assembly_group_reserve_volume(ag, &uuid, 2, &av[2], sto) != 0)
        goto failed;

    /* XXX Not nice */
    memcpy(&ssg->assembly_group, ag, sizeof(assembly_group_t));

    return ssg;

failed:
    os_free(ssg);

    assembly_group_cleanup(ag);
    os_free(ag);

    return NULL;
}

ut_setup()
{
    const uint32_t chunk_size = 262144 / 512; /* Default value, in sectors */
    int i;

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

    ssg = make_fake_ssg(sto);
    UT_ASSERT(ssg != NULL);

    UT_ASSERT_EQUAL(0, memory_stream_open(&memory_stream, __buf, sizeof(__buf),
                                          STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, stat_stream_open(&stream, memory_stream, &stats));
}

ut_cleanup()
{
    int i;

    stream_close(stream);
    stream_close(memory_stream);

    os_free(ssg);

    storage_free(sto);
    for (i = 0; i < NUM_SPOF_GROUPS; i++)
        os_free(rdevs[i]);

    os_random_cleanup();
}

ut_test(serialize_deserialize_is_identity)
{
    sstriping_group_t *ssg2;
    uint64_t computed_size, actual_size;

    UT_ASSERT_EQUAL(0, sstriping_group_serialize(ssg, stream));

    computed_size = sstriping_group_serialized_size(ssg);
    actual_size = stats.write_stats.total_bytes;
    UT_ASSERT_EQUAL(actual_size, computed_size);

    UT_ASSERT_EQUAL(0, stream_rewind(stream));
    UT_ASSERT_EQUAL(0, sstriping_group_deserialize(&ssg2, sto, stream));

    UT_ASSERT(sstriping_group_equals(ssg, ssg2));
}
