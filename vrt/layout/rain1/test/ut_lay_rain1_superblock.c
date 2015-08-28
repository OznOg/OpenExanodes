/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/layout/rain1/src/lay_rain1_superblock.h"
#include "vrt/layout/rain1/src/lay_rain1_rdev.h"

#include "vrt/virtualiseur/fakes/fake_rdev.h"
#include "vrt/virtualiseur/fakes/fake_storage.h"
#include "vrt/virtualiseur/fakes/fake_assembly_group.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/empty_group_definitions.h"
#include "vrt/virtualiseur/fakes/empty_request_definitions.h"

#include "vrt/common/include/memory_stream.h"
#include "vrt/common/include/stat_stream.h"

#include "os/include/os_mem.h"
#include "os/include/os_random.h"

/* FIXME For the fakes below */
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "common/include/uuid.h"

#define RDEV_SIZE        (VRT_SB_AREA_SIZE * 5)

#define NUM_SPOF_GROUPS  3

static struct vrt_realdev *rdevs[NUM_SPOF_GROUPS] = { NULL, NULL, NULL };
static storage_t *sto = NULL;

static rain1_group_t *rxg;

static char __buf[65536];
static stream_t *memory_stream;
static stream_t *stream;
static stream_stats_t stats;

/* XXX Probably badly leaking */
static rain1_group_t *make_fake_rxg(storage_t *sto)
{
    const uint32_t slot_width = NUM_SPOF_GROUPS;
    assembly_group_t *ag = NULL;
    assembly_volume_t *av[3] = { NULL, NULL, NULL };
    exa_uuid_t uuid;
    rain1_group_t *rxg = NULL;
    storage_rdev_iter_t iter;
    vrt_realdev_t *rdev;
    int i;

    rxg = rain1_group_alloc();
    if (rxg == NULL)
        goto failed;

    rxg->nb_rain1_rdevs = storage_get_num_realdevs(sto);

    i = 0;
    storage_rdev_iterator_begin(&iter, sto);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        rxg->rain1_rdevs[i] = rain1_alloc_rdev_layout_data(rdev);
        i++;
    }
    storage_rdev_iterator_end(&iter);

    /* Setup a valid assembly group (containing rdevs, etc) */
    ag = make_fake_ag(sto, slot_width);
    if (ag == NULL)
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
    memcpy(&rxg->assembly_group, ag, sizeof(assembly_group_t));

    return rxg;

failed:
    rain1_group_free(rxg, sto);
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
        rdevs[i]->index = i;
        UT_ASSERT(rdevs[i] != NULL);
    }

    sto = make_fake_storage(NUM_SPOF_GROUPS, chunk_size, rdevs, NUM_SPOF_GROUPS);
    UT_ASSERT(sto != NULL);

    rxg = make_fake_rxg(sto);
    UT_ASSERT(rxg != NULL);

    UT_ASSERT_EQUAL(0, memory_stream_open(&memory_stream, __buf, sizeof(__buf),
                                          STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, stat_stream_open(&stream, memory_stream, &stats));
}

ut_cleanup()
{
    int i;

    stream_close(stream);
    stream_close(memory_stream);

    rain1_group_free(rxg, sto);

    storage_free(sto);
    for (i = 0; i < NUM_SPOF_GROUPS; i++)
        os_free(rdevs[i]);

    os_random_cleanup();
}

ut_test(serialize_deserialize_is_identity)
{
    rain1_group_t *rxg2;
    uint64_t computed_size, actual_size;

    UT_ASSERT_EQUAL(0, rain1_group_serialize(rxg, stream));

    computed_size = rain1_group_serialized_size(rxg);
    actual_size = stats.write_stats.total_bytes;
    UT_ASSERT_EQUAL(actual_size, computed_size);

    UT_ASSERT_EQUAL(0, stream_rewind(stream));
    UT_ASSERT_EQUAL(0, rain1_group_deserialize(&rxg2, sto, stream));

    UT_ASSERT(rain1_group_equals(rxg, rxg2));
}
