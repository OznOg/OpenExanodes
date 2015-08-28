/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/rdev/include/broken_disk_table.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"

#include "os/include/os_file.h"
#include "os/include/os_dir.h"  /* for S_ISREG() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    broken_disk_table_t *table;
    struct stat st;
    int err;
    int i;
    bool broken_found = false;

    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
    {
        const char *self = os_program_name(argv[0]);
        printf("Usage: %s <FILE>\n"
               "\twhere <FILE> is the broken disks file to dump.\n"
               "\n", self);
        exit(1);
    }

    if (stat(argv[1], &st) != 0)
    {
        fprintf(stderr, "Error: file '%s' does not exist.\n", argv[1]);
        exit(1);
    }

    if (!S_ISREG(st.st_mode))
    {
        fprintf(stderr, "Error: '%s' is not a regular file.\n", argv[1]);
        exit(1);
    }

    err = broken_disk_table_load(&table, argv[1], false /* open_read_write */);
    if (err != 0)
    {
        fprintf(stderr, "Error: %s (%d).\n", exa_error_msg(err), err);
        exit(1);
    }

    printf("Table version: %"PRIu64"\n", broken_disk_table_get_version(table));
    for (i = 0; i < NBMAX_DISKS; i++)
    {
        const exa_uuid_t *uuid = broken_disk_table_get_disk(table, i);
        if (uuid != NULL && !uuid_is_zero(uuid))
        {
            printf("Broken disk %d: "UUID_FMT"\n", i, UUID_VAL(uuid));
            broken_found = true;
        }
    }

    if (!broken_found)
        printf("No broken disks.\n");

    broken_disk_table_unload(&table);

    return 0;
}
