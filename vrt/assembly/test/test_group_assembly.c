/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * @file test_group_assembly.c
 *
 * @brief Program that performs a fake group assembly.
 *
 * It contains mock functions that mimic VRT functions that create
 * groups or add devices to groups.
 *
 * The mock functions implement only the basic operations required by
 * the assembly to work. Operations that are not required by the
 * assembly are *not* performed (for instance, write the superblocks,
 * create the rebuilding thread, etc.).
 *
 * For the vrt_group and vrt_realdev structures, only the relevant
 * fields are set. Some of them are generated, like UUIDs.
 */

#include "vrt/assembly/test/test_group_assembly_funcs.h"
#include "vrt/assembly/include/assembly_prediction.h"

#include "vrt/virtualiseur/include/vrt_init.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/storage.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_assert.h"

#include "os/include/os_file.h"
#include "os/include/os_getopt.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

static void usage(const char *self)
{
    printf("Usage: %s [OPTIONS]... DESCRIPTION_FILE\n", self);
    printf("\n");
    printf("Perform a fake assembly of a group described in DESCRIPTION_FILE.\n");
    printf("The description file contains the node ID and the size of each disk.\n\n");
    printf("OPTIONS:\n");
    printf("  -c, --chunk-size SIZE      Chunk size in MB (default: %d MB)\n", VRT_DEFAULT_CHUNK_SIZE >> 10);
    printf("  -s, --slot-width WIDTH     Slot width in chunks (default: %d)\n", DEFAULT_SLOT_WIDTH);
    printf("  -r, --heuristic NAME       Assembly heuristic's name (default: %s)\n", DEFAULT_HEURISTIC_NAME);
    printf("  -d, --dump-assembly FILE   Dump the group assembly as XML data to FILE\n");
    printf("  -h, --help                 Display this help and exit\n");
}

static int write_data(const char *str, const char *filename)
{
    FILE *fd;
    int written;
    int err = 0;

    if ((fd = fopen(filename, "wb")) == NULL)
    {
        fprintf(stderr, "Failed opening '%s': %s", filename, strerror(errno));
        return -errno;
    }

    written = fprintf(fd, "%s", str);
    if (written < 0)
        err = -errno;
    else if (written != strlen(str))
        err = -EIO;

    fclose(fd);

    return err;
}

int main(int argc, char *argv[])
{
    const char *self;
    struct assembly_group *ag;
    uint32_t chunk_size;
    uint32_t slot_width;
    char *heuristic_name;
    char *dump_name;
    int long_index;
    struct vrt_realdev *rdevs = NULL;
    unsigned int rdev_count, i;
    uint64_t *spof_chunks;
    unsigned num_spofs;
    uint64_t predicted_max_slots;
    storage_t *sto;
    int err_chunks;
    int err_prediction;
    int err_dump;

    struct option opts[] =
	{
	    { "chunk-size",    required_argument, NULL, 'c' },
	    { "slot-width",    required_argument, NULL, 's' },
	    { "heuristic",     required_argument, NULL, 'r' },
	    { "dump-assembly", required_argument, NULL, 'd' },
	    { "help",          no_argument,       NULL, 'h' },
	    { NULL,            0,                 NULL, 0   }
	};

    self = os_basename(argv[0]);

    chunk_size = VRT_DEFAULT_CHUNK_SIZE;
    slot_width = DEFAULT_SLOT_WIDTH;
    heuristic_name = DEFAULT_HEURISTIC_NAME;
    dump_name = NULL;

    while (TRUE)
    {
        int c = os_getopt_long(argc, argv, "c:s:r:d:h", opts, &long_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'c':
            if (to_uint32(optarg, &chunk_size) != 0)
            {
                fprintf(stderr, "Invalid chunk size: '%s'\n", optarg);
                exit(EXIT_FAILURE);
            }
            chunk_size <<= 10; /* Convert from MB to KB */
            break;
        case 's':
            if (to_uint32(optarg, &slot_width) != 0)
            {
                fprintf(stderr, "Invalid slot width: '%s'\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'r':
            heuristic_name = optarg;
            break;
        case 'd':
            dump_name = optarg;
            break;
        case 'h':
            usage(self);
            return EXIT_SUCCESS;
            break;
        }
    }

    if (argc - optind != 1)
    {
        usage(self);
        return EXIT_FAILURE;
    }

    rdevs = read_rdevs_from_file(argv[argc - 1], &rdev_count);
    if (rdevs == NULL)
        return -1;

    sto = make_storage(rdevs, rdev_count, chunk_size);

    ag = make_assembly_group(rdevs, rdev_count, sto, chunk_size, slot_width,
                             heuristic_name, TRUE);
    if (ag == NULL)
        return -1;

    err_chunks = compute_per_spof_chunks(rdevs, rdev_count, chunk_size,
                                         &spof_chunks, &num_spofs);
    if (err_chunks != 0)
    {
        fprintf(stderr, "Failed computing per-spof chunks: %s (%d)\n",
                strerror(-err_chunks), err_chunks);
        return EXIT_FAILURE;
    }

    printf("\n");
    printf("= SPOF information =\n");
    printf("Number of spofs = %u\n", num_spofs);
    for (i = 0; i < num_spofs; i++)
        printf("  spof %u: %"PRIu64" chunks\n", i, spof_chunks[i]);

    predicted_max_slots =
        assembly_predict_max_slots_without_sparing(num_spofs, slot_width,
                                                   spof_chunks);

    printf("Predicted max slots = %"PRIu64", expected %"PRIu64,
           predicted_max_slots, assembly_group_get_max_slots_count(ag, sto));
    if (predicted_max_slots == assembly_group_get_max_slots_count(ag, sto))
    {
        printf(" => OK\n");
        err_prediction = 0;
    }
    else
    {
        printf(" => WRONG\n");
        err_prediction = 1;
    }

    os_free(spof_chunks);

    if (dump_name != NULL)
    {
        char *str;
        assembly_volume_t *av;
        exa_uuid_t uuid;
        /* Assemble all slots */
        uuid_zero(&uuid);
        assembly_group_reserve_volume(ag, &uuid,
                assembly_group_get_max_slots_count(ag, sto), &av, sto);

        str = serialize_group_assembly_to_xml(ag, av, sto, rdevs, rdev_count);

        err_dump = write_data(str, dump_name);
        os_free(str);

        if (err_dump == EXA_SUCCESS)
            printf("The group assembly was successfully written to '%s'.\n",
                   dump_name);
        else
            fprintf(stderr, "An error occurred during the dump of the assembly: %s (%d)\n",
                    strerror(-err_dump), err_dump);
    }
    else
        err_dump = 0;

    for (i = 0 ; i < rdev_count; i++)
        extent_list_free(rdevs[i].chunks.free_chunks);

    os_free(rdevs);
    os_free(ag);

    if (err_chunks || err_prediction || err_dump != 0)
        return 1;

    return 0;
}
