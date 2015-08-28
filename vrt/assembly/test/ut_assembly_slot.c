/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/virtualiseur/fakes/fake_rdev.h"

#include "vrt/assembly/src/assembly_slot.h"

#include "vrt/virtualiseur/include/storage.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"

#include "vrt/common/include/memory_stream.h"
#include "vrt/common/include/spof.h"

#include "common/include/exa_math.h"
#include "common/include/uuid.h"

#include "os/include/os_mem.h"
#include "os/include/os_random.h"

static storage_t *sto;
static char buf[1392];
static stream_t *stream;

ut_setup()
{
    exa_uuid_t uuids[5];
    vrt_realdev_t *rdevs[5];
    int i;

    os_random_init();

    /* Fake rdevs */

    for (i = 0; i < 5; i++)
        uuid_generate(&uuids[i]);

    rdevs[0] = make_fake_rdev(0, 1, &uuids[0], &uuids[0], 500000, true, true);
    rdevs[1] = make_fake_rdev(1, 1, &uuids[1], &uuids[1], 500000, true, true);
    rdevs[2] = make_fake_rdev(2, 2, &uuids[2], &uuids[2], 500000, true, true);
    rdevs[3] = make_fake_rdev(3, 3, &uuids[3], &uuids[3], 500000, true, true);
    rdevs[4] = make_fake_rdev(4, 3, &uuids[4], &uuids[4], 500000, true, true);

    /* Storage */

    sto = storage_alloc();
    UT_ASSERT(sto != NULL);

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 1, rdevs[0]));
    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 1, rdevs[1]));

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 2, rdevs[2]));

    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 3, rdevs[3]));
    UT_ASSERT_EQUAL(0, storage_add_rdev(sto, 3, rdevs[4]));

    UT_ASSERT_EQUAL(0, storage_cut_in_chunks(sto, 128*512));

    /* Stream for serialization */

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, buf, sizeof(buf), STREAM_ACCESS_RW));
}

ut_cleanup()
{
    storage_free(sto);
    stream_close(stream);

    os_random_cleanup();
}

ut_test(serialize_deserialize_is_identity)
{
    slot_t *slot, *slot2;
    uint32_t slot_width;

    /* MIN taken from rainX_group_create() */
    slot_width = MIN(sto->num_spof_groups, 6);
    slot = slot_make(sto->spof_groups, sto->num_spof_groups, slot_width);

    UT_ASSERT(slot != NULL);

    slot_serialize(slot, stream);

    stream_rewind(stream);

    slot2 = NULL;
    slot_deserialize(&slot2, sto, stream);
    UT_ASSERT(slot2 != NULL);

    UT_ASSERT(slot_equals(slot2, slot));

    slot_free(slot);
    slot_free(slot2);
}
