/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/fakes/fake_rdev.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/fake_assembly_group.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"

#include "vrt/virtualiseur/include/storage.h"

#include "vrt/common/include/memory_stream.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/os_random.h"

/* FIXME - FAKES - THAT'S HORRENDOUS */
#define RDEV_SIZE        (VRT_SB_AREA_SIZE * 5)

#define NUM_SPOF_GROUPS  3

static struct vrt_realdev *rdevs[NUM_SPOF_GROUPS];
static storage_t *sto = NULL;

static assembly_group_t *ag;
static assembly_volume_t *av[3];

static void __common_setup(void)
{
    const uint32_t slot_width = NUM_SPOF_GROUPS;
    const uint32_t chunk_size = 262144 / 512; /* Default value, in sectors */
    exa_uuid_t uuid;
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

    /* Setup a valid assembly group (containing rdevs, etc) */
    ag = make_fake_ag(sto, slot_width);
    UT_ASSERT(ag != NULL);

    uuid_generate(&uuid);
    UT_ASSERT_EQUAL(0, assembly_group_reserve_volume(ag, &uuid, 5, &av[0], sto));
    UT_ASSERT(av[0] != NULL);

    uuid_generate(&uuid);
    UT_ASSERT_EQUAL(0, assembly_group_reserve_volume(ag, &uuid, 7, &av[1], sto));
    UT_ASSERT(av[1] != NULL);

    uuid_generate(&uuid);
    UT_ASSERT_EQUAL(0, assembly_group_reserve_volume(ag, &uuid, 2, &av[2], sto));
    UT_ASSERT(av[2] != NULL);
}

static void __common_cleanup(void)
{
    int i;

    assembly_group_cleanup(ag);
    os_free(ag);

    storage_free(sto);

    /* FIXME Should use vrt_realdev_free() but can we use the real
       vrt_realdev.c here and now? */
    for (i = 0; i < NUM_SPOF_GROUPS; i++)
        os_free(rdevs[i]);

    os_random_cleanup();
}

UT_SECTION(lookup_assembly_volume)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(lookup_unknown_uuid_returns_null)
{
    exa_uuid_t uuid;

    uuid_generate(&uuid);
    UT_ASSERT(assembly_group_lookup_volume(ag, &uuid) == NULL);
}

ut_test(lookup_known_uuid_returns_assembly_volume)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        exa_uuid_t *uuid = &av[i]->uuid;
        UT_ASSERT(assembly_group_lookup_volume(ag, uuid) == av[i]);
    }
}

UT_SECTION(serialization)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(serialize_deserialize_is_identity)
{
    assembly_group_t ag2;
#define BUF_SIZE (50 * 1024) /* bytes */
    char buf[BUF_SIZE];
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, buf, BUF_SIZE, STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, assembly_group_serialize(ag, stream));

    stream_rewind(stream);
    UT_ASSERT_EQUAL(0, assembly_group_deserialize(&ag2, sto, stream));

    stream_close(stream);

    UT_ASSERT(assembly_group_equals(&ag2, ag));
}
