/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include <unit_testing.h>
#include <sys/stat.h>

#include "vrt/assembly/test/test_group_assembly_funcs.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"

static char *load_text_file(const char *path)
{
    struct stat st;
    char *buffer;
    FILE *fp;

    UT_ASSERT_EQUAL(0, stat(path, &st));

    buffer = os_malloc(st.st_size + 1);
    fp = fopen(path, "rb");
    UT_ASSERT_EQUAL(1, fread(buffer, st.st_size, 1, fp));

    buffer[st.st_size] = '\0';

    fclose(fp);
    return buffer;
}

static void assembly_group_regression_check(const char *file)
{
    struct assembly_group *ag;
    uint32_t chunk_size;
    uint32_t slot_width;
    storage_t *sto;
    char *heuristic_name;
    struct vrt_realdev *rdevs = NULL;
    unsigned int rdev_count;
    char *computed_buffer, *reference_buffer;
    char def_path[OS_PATH_MAX], expected_result_path[OS_PATH_MAX];
    char *srcdir = CURRENT_SOURCE_DIR;
    assembly_volume_t *av;
    exa_uuid_t uuid;

    os_snprintf(def_path, sizeof(def_path), "%s%s%s",
                srcdir ? srcdir:".", OS_FILE_SEP, file);

    os_snprintf(expected_result_path, sizeof(expected_result_path), "%s.result",
                def_path);

    chunk_size = VRT_DEFAULT_CHUNK_SIZE;
    slot_width = DEFAULT_SLOT_WIDTH;
    heuristic_name = DEFAULT_HEURISTIC_NAME;

    rdevs = read_rdevs_from_file(def_path, &rdev_count);
    UT_ASSERT(rdevs != NULL);

    sto = make_storage(rdevs, rdev_count, chunk_size);

    ag = make_assembly_group(rdevs, rdev_count, sto,
                             chunk_size, slot_width,
                             heuristic_name, false);
    UT_ASSERT(ag != NULL);

    /* Assemble all slots */
    uuid_zero(&uuid);
    assembly_group_reserve_volume(ag, &uuid,
                                 assembly_group_get_available_slots_count(ag, sto),
                                 &av, sto);

    computed_buffer = serialize_group_assembly_to_xml(ag, av, sto, rdevs, rdev_count);
    UT_ASSERT(computed_buffer != NULL);

    reference_buffer = load_text_file(expected_result_path);
    UT_ASSERT(reference_buffer != NULL);

    UT_ASSERT_EQUAL_STR(reference_buffer, computed_buffer);

    os_free(computed_buffer);
    os_free(reference_buffer);
}

ut_test(106_nodes_311_disks)
{
    assembly_group_regression_check("106_nodes_311_disks.txt");
}

ut_test(11_nodes_63_disks)
{
    assembly_group_regression_check("11_nodes_63_disks.txt");
}

ut_test(32_nodes_32_disks)
{
    assembly_group_regression_check("32_nodes_32_disks.txt");
}

ut_test(3_nodes_3_disks)
{
    assembly_group_regression_check("3_nodes_3_disks.txt");
}

ut_test(3_nodes_6_disks)
{
    assembly_group_regression_check("3_nodes_6_disks.txt");
}

ut_test(5_nodes_9_disks)
{
    assembly_group_regression_check("5_nodes_9_disks.txt");
}

ut_test(8_nodes_512_disks)
{
    assembly_group_regression_check("8_nodes_512_disks.txt");
}
