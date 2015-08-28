/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * @file test_group_assembly_funcs.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "os/include/os_getopt.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_assert.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"
#include "vrt/virtualiseur/include/vrt_init.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"

#include "vrt/assembly/test/test_group_assembly_funcs.h"
#include "vrt/assembly/src/assembly_group.h"

static void print_group_info(const struct assembly_group *ag, const storage_t *sto)
{
    uint64_t max_slots_count = assembly_group_get_max_slots_count(ag, sto);
    printf("= Information about the group's assembly =\n"
           "Number of slots  = %" PRIu64 "\n"
           "Number of chunks = %" PRIu64 "\n"
           "Slot width       = %" PRIu32 "\n"
           "Slot size        = %" PRIu32 " sectors\n"
           "Physical size    = %" PRIu64 " GB\n\n",
           max_slots_count,
           max_slots_count * ag->slot_width,
           ag->slot_width,
           ag->slot_size,
           max_slots_count * ag->slot_size >> 21);
}


static void print_rdevs_info(const struct vrt_realdev *rdevs, unsigned int rdev_count)
{
    uint32_t i;

    printf("= Information about devices =\n");
    for (i = 0; i < rdev_count; i++)
    {
        const struct vrt_realdev *rdev = &rdevs[i];
        uint32_t used_chunks_count = rdev->chunks.total_chunks_count - rdev->chunks.free_chunks_count;

        printf("rdev: index = %d, node id = %"PRInodeid", size = %" PRIu64 " MB, "
               "chunks used = [%" PRIu32 "/%" PRIu32 "]"
               " %" PRIu32 " %%\n",
               rdev->index, rdev->node_id, rdev->real_size >> 11,
               used_chunks_count, rdev->chunks.total_chunks_count,
               used_chunks_count * 100 / rdev->chunks.total_chunks_count);
    }
    printf("\n");
}


static void print_memory_usage(const struct assembly_group *ag, const storage_t *sto)
{
    uint64_t slots_memory;
    uint64_t chunks_memory;
    uint64_t max_slots_count = assembly_group_get_max_slots_count(ag, sto);

    slots_memory  = max_slots_count * sizeof(struct slot);
    chunks_memory = max_slots_count * ag->slot_width * sizeof(struct chunk);

    printf("= Memory usage of the group's assembly =\n"
           "Slots  = %" PRIu64 " bytes\n"
           "Chunks = %" PRIu64 " bytes\n"
           "Total  = %" PRIu64 " bytes\n",
           slots_memory, chunks_memory, slots_memory + chunks_memory);
}

/**
 * Append a string to another, reallocating the destination string as needed.
 *
 * @param[in,out] dest      Destination string
 * @param[in]     append    String to append
 * @param[in,out] dest_end  Offset of destination's end (terminal '\0')
 *
 * @return result of the append (replaces destination)
 */
static char *str_append(char *dest, const char *append, size_t *dest_end)
{
    char *buffer;
    size_t dest_len = 0;
    size_t append_len = 0;
    size_t size = 0;

    if (append == NULL)
        return dest;

    dest_len = *dest_end;
    append_len = strlen(append);

    size = dest_len + append_len + 1;

    buffer = os_realloc(dest, size);

    strcpy(buffer + dest_len, append);

    buffer[size - 1] = '\0';
    *dest_end = size - 1;

    return buffer;
}

char *serialize_group_assembly_to_xml(assembly_group_t *ag,
                                      assembly_volume_t *av,
                                      const storage_t *sto,
                                      struct vrt_realdev *rdevs,
                                      unsigned int rdev_count)
{
    uint64_t i;
    uint32_t j;
    char *buffer;
    size_t buffer_end = 0;
    char tmp[512];
    uint64_t max_slots_count = assembly_group_get_max_slots_count(ag, sto);

    buffer = str_append(NULL, "<?xml version=\"1.0\"?>\n<assembly>\n", &buffer_end);

    os_snprintf(tmp, 512, "\t<groupinfo num_slots=\"%"PRIu64"\" num_chunks=\"%"PRIu64"\"\n"
                          "\t           slot_width=\"%"PRIu32"\" slot_size_sec=\"%"PRIu32"\"\n"
                          "\t           physical_size=\"%"PRIu64"\"/>\n\n",
                max_slots_count,
                max_slots_count * ag->slot_width,
                ag->slot_width,
                ag->slot_size,
                max_slots_count * ag->slot_size);

    buffer = str_append(buffer, tmp, &buffer_end);

    for (i = 0; i < av->total_slots_count; i++)
    {
        buffer = str_append(buffer, "\t<slot>\n", &buffer_end);

        for (j = 0; j < ag->slot_width; j++)
        {
            const chunk_t *chunk = av->slots[i]->chunks[j];
            const struct vrt_realdev *rdev = chunk_get_rdev(chunk);

            os_snprintf(tmp, 512, "\t\t<chunk rdev_index=\"%d\" chunk_id=\"%" PRIu64 "\"/>\n",
                    rdev->index, chunk_get_offset(chunk) / rdev->chunks.chunk_size);

            buffer = str_append(buffer, tmp, &buffer_end);
        }

        buffer = str_append(buffer, "\t</slot>\n", &buffer_end);
    }

    buffer = str_append(buffer, "\n", &buffer_end);

    for (i = 0; i < rdev_count; i++)
    {
        const struct vrt_realdev *rdev = &rdevs[i];
        uint32_t used_chunks_count = rdev->chunks.total_chunks_count - rdev->chunks.free_chunks_count;

        os_snprintf(tmp, 512, "\t<rdev index=\"%d\" size=\"%"PRIu64"\"\n"
                              "\t      chunks_used=\"%"PRIu32"\" chunks_total=\"%"PRIu32"\"/>\n",
                    rdev->index, rdev->real_size,
                    used_chunks_count, rdev->chunks.total_chunks_count);

        buffer = str_append(buffer, tmp, &buffer_end);
    }

    buffer = str_append(buffer, "</assembly>\n", &buffer_end);

    assembly_group_release_volume(ag, av, sto);

    return buffer;
}


/**
 * Read a list of rdevs from a file.
 *
 * @param[in]  filename    File to read
 * @param[out] rdev_count  Number of rdevs read
 *
 * @return array of rdevs read if successful, NULL otherwise
 */
struct vrt_realdev *read_rdevs_from_file(const char *filename,
                                         unsigned int *rdev_count)
{
    FILE *fd;
    char buf[128];
    unsigned int cnt = 0;
    struct vrt_realdev *rdevs = NULL;

    if (rdev_count == NULL)
        return NULL;

    *rdev_count = 0;

    if ((fd = fopen(filename, "r")) == NULL)
    {
        perror("fopen");
        return NULL;
    }

    while ((fgets(buf, sizeof(buf), fd) != NULL))
    {
        size_t len = strlen(buf);
        char *comment;
        exa_nodeid_t node_id;
        uint64_t rdev_size;

        /* Remove trailing '\n' */
        if (len > 0 && buf[len - 1] == '\n')
            buf[len - 1] = '\0';

        /* Comments (starting with #) and blank lines are ignored */
        os_str_trim(buf);

        comment = strchr(buf, '#');
        if (comment != NULL)
            *comment = '\0';

        if (buf[0] == '\0')
            continue;

        if (sscanf(buf, "%"PRInodeid"\t%"PRIu64, &node_id, &rdev_size) != 2)
        {
            fprintf(stderr, "Failed to process line: '%s'\n", buf);
            os_free(rdevs);
            cnt = 0;
            break;
        }

        rdevs = os_realloc(rdevs, sizeof(struct vrt_realdev) * (cnt + 1));
        memset(&rdevs[cnt], 0, sizeof(struct vrt_realdev));
        rdevs[cnt].real_size = rdev_size * 2; /* Convert from KB to sectors. */
        rdevs[cnt].node_id = node_id;
        rdevs[cnt].index   = cnt;

        cnt++;
    }

    fclose(fd);

    *rdev_count = cnt;

    return rdevs;
}

/**
 * XXX This function is duplicates in test_group_assembly_funcs.c because of
 * linking problems.
 * FIXME Careful this function assumes that spof ids are contiguous.
 *
 * XXX This code deserve to be move in some common storage part. */
spof_group_t *make_spof_groups(struct vrt_realdev *rdevs, uint32_t rdev_count,
                               uint32_t *nb_spof_groups)
{
    int i;
    int last_node_id = 0;
    spof_group_t *spof_groups;

    for (i = 0; i < rdev_count; i++)
        if (rdevs[i].node_id > last_node_id)
            last_node_id = rdevs[i].node_id;

    spof_groups = os_malloc((last_node_id + 1) * sizeof(spof_group_t));

    for (i = 0; i < last_node_id + 1; i++)
        spof_groups[i].nb_realdevs = 0;

    for (i = 0; i < rdev_count; i++)
    {
        int idx = rdevs[i].node_id;
        spof_groups[idx].spof_id = rdevs[i].node_id + 1;
        spof_groups[idx].realdevs[spof_groups[idx].nb_realdevs++] = &rdevs[i];
    }

    *nb_spof_groups = last_node_id + 1;

    return spof_groups;
}

storage_t *make_storage(struct vrt_realdev *rdevs, unsigned int rdev_count,
                        uint32_t chunk_size /* in bytes */)
{
    storage_t *sto = storage_alloc();
    int i;

    if (sto == NULL)
        return NULL;

    for (i = 0; i < rdev_count; i++)
    {
        rdevs[i].spof_id = rdevs[i].node_id + 1;
        storage_add_rdev(sto, rdevs[i].spof_id, &rdevs[i]);
    }

    storage_cut_in_chunks(sto, SECTORS_2_KBYTES(chunk_size));

    return sto;
}

struct assembly_group *make_assembly_group(struct vrt_realdev *rdevs,
                                           unsigned int rdev_count,
                                           const storage_t *sto,
                                           uint32_t chunk_size, /* in bytes */
                                           uint32_t slot_width,
                                           const char *heuristic_name,
                                           bool verbose)
{
    struct assembly_group *ag;
    int err;

    ag = os_malloc(sizeof(struct assembly_group));
    if (ag == NULL)
        return NULL;

    assembly_group_init(ag);

    err = assembly_group_setup(ag, slot_width, chunk_size);

    if (err != EXA_SUCCESS)
        goto cleanup;

    if (verbose)
    {
        print_group_info(ag, sto);
        print_rdevs_info(rdevs, rdev_count);
        print_memory_usage(ag, sto);
    }

    return ag;

cleanup:
    os_free(ag);

    return NULL;
}

int compute_per_spof_chunks(const struct vrt_realdev *rdevs, unsigned rdev_count,
                            uint32_t chunk_size,
                            uint64_t **spof_chunks, unsigned *spof_count)
{
    uint64_t chunks[EXA_MAX_NODES_NUMBER];
    unsigned i, k;

    EXA_ASSERT(rdevs != NULL);
    EXA_ASSERT(spof_chunks != NULL);
    EXA_ASSERT(spof_count != NULL);

    /* Compute number of chunks per spof */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        chunks[i] = 0;

    for (i = 0; i < rdev_count; i++)
        chunks[rdevs[i].node_id] += vrt_realdev_get_usable_size(&rdevs[i])
                                    / chunk_size;

    /* Compute the actual number of spofs */
    *spof_count = 0;
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        if (chunks[i] > 0)
            (*spof_count)++;

    /* Allocate and fill in the resulting array of per-spof chunks */
    *spof_chunks = os_malloc(sizeof(uint64_t) * *spof_count);
    if (*spof_chunks == NULL)
        return -ENOMEM;

    k = 0;
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        if (chunks[i] > 0)
            (*spof_chunks)[k++] = chunks[i];

    return 0;
}
