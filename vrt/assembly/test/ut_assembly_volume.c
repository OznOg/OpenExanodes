/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/fakes/fake_assembly_group.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/fake_rdev.h"

#include "vrt/assembly/src/assembly_volume.h"
#include "vrt/common/include/memory_stream.h"

#include "os/include/os_random.h"

#include "os/include/os_mem.h"

static stream_t *stream;
static char buf[100000];

ut_setup()
{
    os_random_init();
    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, buf, sizeof(buf), STREAM_ACCESS_RW));
}

ut_cleanup()
{
    stream_close(stream);
    os_random_cleanup();
}

static bool __contains(uint64_t *elems, int n_elems, uint64_t value)
{
    int i;

    for (i = 0; i < n_elems; i++)
        if (elems[i] == value)
            return true;

    return false;
}

/* FIXME - FAKES - THAT'S HORRENDOUS */
#define RDEV_SIZE        (VRT_SB_AREA_SIZE * 5)
#define NUM_SPOF_GROUPS  3
static storage_t *sto = NULL;
static struct vrt_realdev *rdevs[NUM_SPOF_GROUPS];
const uint32_t chunk_size = 262144 / 512; /* Default value, in sectors */

ut_test(serialize_deserialize_is_id)
{
#define NUM_SLOTS  100
    uint64_t slot_indexes[NUM_SLOTS];
    exa_uuid_t uuid;
    assembly_group_t *ag;
    assembly_volume_t *av, *av2;
    int i;

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

    /* Fill in the slot indexes with unique values */
    for (i = 0; i < NUM_SLOTS; i++)
    {
        uint64_t n;

        do
            /* 623 arbitrarily picked */
            n = (uint64_t)(os_drand() * 623);
        while (__contains(slot_indexes, i, n));

        slot_indexes[i] = n;
    }

    /* Setup a valid assembly group (containing rdevs, etc) */
    ag = make_fake_ag(sto, 3 /*slot_width*/);
    UT_ASSERT(ag != NULL);

    uuid_generate(&uuid);
    UT_ASSERT_EQUAL(0, assembly_group_reserve_volume(ag, &uuid, NUM_SLOTS, &av, sto));
    UT_ASSERT(av != NULL);

    UT_ASSERT_EQUAL(0, assembly_volume_serialize(av, stream));

    stream_rewind(stream);
    UT_ASSERT_EQUAL(0, assembly_volume_deserialize(&av2, sto, stream));

    UT_ASSERT(assembly_volume_equals(av2, av));

    assembly_volume_free(av2);

    assembly_group_cleanup(ag);
    os_free(ag);
}
