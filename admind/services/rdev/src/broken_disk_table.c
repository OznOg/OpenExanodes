/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/rdev/include/broken_disk_table.h"

#include "common/include/checksum.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_mkstr.h"

#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_filemap.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

#define BROKEN_DISK_TABLE_MAGIC 0xB03DABEAB03DABEA

struct broken_disk_data
{
    uint64_t magic;
    exa_uuid_t uuids[NBMAX_DISKS];
    uint64_t version;
    checksum_t checksum;
};

/** Size of fmap file */
#define BROKEN_DISK_TABLE_FMAP_SIZE (sizeof(struct broken_disk_data))

struct broken_disk_table
{
    struct broken_disk_data data;
    os_fmap_t *fmap;
};

static int broken_disk_table_create(broken_disk_table_t *table, const char *filename)
{
    int err;
    EXA_ASSERT(table != NULL && filename != NULL);

    EXA_ASSERT(table->fmap == NULL);

    if (os_fmap_access(table->fmap) == FMAP_READ)
        return -EPERM;

    table->fmap = os_fmap_create(filename, BROKEN_DISK_TABLE_FMAP_SIZE);
    if (table->fmap == NULL)
        return -EPERM;

    err = broken_disk_table_clear(table);
    if (err != 0)
        return err;

    err = broken_disk_table_set_version(table, 1);
    if (err != 0)
        return err;

    table->data.magic = BROKEN_DISK_TABLE_MAGIC;

    return broken_disk_table_write(table);
}

/**
 * Read the broken disk table from disk.
 *
 * (Synchronizes the in-memory version of the table with the on-disk
 * version.)
 *
 * @param[in,out] table  Broken disk table to read
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int broken_disk_table_read(broken_disk_table_t *table)
{
    struct broken_disk_data *disk_data;

    if (table == NULL || table->fmap == NULL)
        return -EINVAL;

    disk_data = os_fmap_addr(table->fmap);

    memcpy(&table->data, disk_data, sizeof(table->data));

    if (table->data.magic != BROKEN_DISK_TABLE_MAGIC)
        return -RDEV_ERR_INVALID_BROKEN_DISK_TABLE;

    if (table->data.version == 0)
        return -RDEV_ERR_INVALID_BROKEN_DISK_TABLE;

    if (exa_checksum(&table->data, sizeof(table->data)) != 0)
        return -RDEV_ERR_INVALID_BROKEN_DISK_TABLE;

    return 0;
}

int broken_disk_table_load(broken_disk_table_t **table, const char *filename,
                           bool open_read_write)
{
    fmap_access_t access;
    int err;

    if (table == NULL || filename == NULL)
        return -EINVAL;

    *table = os_malloc(sizeof(broken_disk_table_t));
    if (*table == NULL)
        return -ENOMEM;

    access = open_read_write ? FMAP_RDWR : FMAP_READ;

    (*table)->fmap = os_fmap_open(filename, BROKEN_DISK_TABLE_FMAP_SIZE, access);
    if ((*table)->fmap == NULL)
    {
        if (!open_read_write)
            err = -ENOENT;
        else
            err = broken_disk_table_create(*table, filename);

        if (err != 0)
        {
            os_free(*table);
            *table = NULL;
            return err;
        }
    }
    else
    {
        err = broken_disk_table_read(*table);
        if (err != 0)
        {
            broken_disk_table_unload(table);
            return err;
        }
    }

    return 0;
}

void broken_disk_table_unload(broken_disk_table_t **table)
{
    if (table == NULL || *table == NULL)
        return;

    if ((*table)->fmap != NULL)
    {
        os_fmap_close((*table)->fmap);
        os_free(*table);
        *table = NULL;
    }
}

int broken_disk_table_write(broken_disk_table_t *table)
{
    struct broken_disk_data *disk_data;

    if (table == NULL || table->fmap == NULL)
        return -EINVAL;

    if (os_fmap_access(table->fmap) == FMAP_READ)
        return -EPERM;

    EXA_ASSERT(table->data.version != 0);
    EXA_ASSERT(table->data.magic == BROKEN_DISK_TABLE_MAGIC);

    table->data.checksum = 0;
    table->data.checksum = exa_checksum(&table->data, sizeof(table->data));

    disk_data = os_fmap_addr(table->fmap);

    memcpy(disk_data, &table->data, sizeof(table->data));

    return os_fmap_sync(table->fmap);
}

int broken_disk_table_set_disk(broken_disk_table_t *table, int index,
                               const exa_uuid_t *disk_uuid)
{
    if (table == NULL || table->fmap == NULL)
        return -EINVAL;

    if (index < 0 || index > NBMAX_DISKS)
        return -EINVAL;

    if (disk_uuid == NULL)
        return -EINVAL;

    if (os_fmap_access(table->fmap) == FMAP_READ)
        return -EPERM;

    uuid_copy(&table->data.uuids[index], disk_uuid);

    return 0;
}

const exa_uuid_t *broken_disk_table_get_disk(const broken_disk_table_t *table,
                                             int index)
{
    if (table == NULL || table->fmap == NULL)
        return NULL;

    if (index < 0 || index > NBMAX_DISKS)
        return NULL;

    return &table->data.uuids[index];
}

int broken_disk_table_set(broken_disk_table_t *table, const exa_uuid_t *uuids)
{
    if (table == NULL || table->fmap == NULL)
        return -EINVAL;

    if (uuids == NULL)
        return -EINVAL;

    if (os_fmap_access(table->fmap) == FMAP_READ)
        return -EPERM;

    memcpy(table->data.uuids, uuids, sizeof(table->data.uuids));

    return broken_disk_table_write(table);
}

const exa_uuid_t *broken_disk_table_get(const broken_disk_table_t *table)
{

    if (table == NULL || table->fmap == NULL)
        return NULL;

    return table->data.uuids;
}

bool broken_disk_table_contains(const broken_disk_table_t *table,
                                const exa_uuid_t *disk_uuid)
{
    int i;

    if (table == NULL || table->fmap == NULL)
        return false;

    if (disk_uuid == NULL)
        return false;

    for (i = 0; i < NBMAX_DISKS; i++)
        if (uuid_is_equal(&table->data.uuids[i], disk_uuid))
            return true;

    return false;
}

int broken_disk_table_clear(broken_disk_table_t *table)
{
    int i;
    int err = 0;

    for (i = 0; i < NBMAX_DISKS && err == 0; i++)
        err = broken_disk_table_set_disk(table, i, &exa_uuid_zero);

    return err;
}

uint64_t broken_disk_table_get_version(const broken_disk_table_t *table)
{
    if (table == NULL)
        return 0;

    return table->data.version;
}

int broken_disk_table_set_version(broken_disk_table_t *table, uint64_t version)
{
    if (table == NULL)
        return -EINVAL;

    if (os_fmap_access(table->fmap) == FMAP_READ)
        return -EPERM;

    table->data.version = version;

    return 0;
}

int broken_disk_table_increment_version(broken_disk_table_t *table)
{
    if (table == NULL)
        return -EINVAL;

    if (os_fmap_access(table->fmap) == FMAP_READ)
        return -EPERM;

    table->data.version++;

    /* FIXME wrapping */
    if (table->data.version == 0)
        table->data.version++;

    return 0;
}
