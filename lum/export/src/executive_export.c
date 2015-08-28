/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "lum/export/src/export_io.h"
#include "lum/export/include/executive_export.h"
#include "lum/export/include/export.h"

/* XXX Not nice */
#include "vrt/virtualiseur/src/vrt_module.h"

#include "target/include/target_adapter.h"

#include "log/include/log.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"

#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/strlcpy.h"

#include "target/iscsi/include/target.h"
#include "target/iscsi/include/lun.h"

#include <errno.h>

struct lum_export {
    struct lum_export *next;
    blockdevice_t *blockdevice;
    export_t *desc;
    const target_adapter_t * target_adapter;
};

/* linked list of all exports */
static lum_export_t *lum_exports = NULL;

int lum_export_static_init(exa_nodeid_t node_id)
{
    int err;

    err = export_io_static_init();
    if (err != EXA_SUCCESS)
        return err;

    err = get_iscsi_adapter()->static_init(node_id);
#ifdef WITH_BDEV
    if (err != EXA_SUCCESS)
        return err;

    return get_bdev_adapter()->static_init(node_id);
#else
    return err;
#endif
}

void lum_export_static_cleanup(void)
{
    get_iscsi_adapter()->static_cleanup();
#ifdef WITH_BDEV
    get_bdev_adapter()->static_cleanup();
#endif

    export_io_static_cleanup();
}

static void lum_export_add(lum_export_t *export)
{
    export->next = lum_exports;
    lum_exports = export;
}

static void lum_export_remove(lum_export_t *an_export)
{
    lum_export_t *export;

    /* if element is in front of list */
    if (an_export == lum_exports)
    {
        lum_exports = an_export->next;
        return;
    }

    for (export = lum_exports; export != NULL; export = export->next)
    {
        if (export->next != NULL && export->next == an_export)
        {
            export->next = an_export->next;
            return;
        }
    }
    EXA_ASSERT_VERBOSE(false, "Cannot find export uuid=" UUID_FMT,
                       UUID_VAL(export_get_uuid(an_export->desc)));
}

static lum_export_t *lum_export_find_by_uuid(const exa_uuid_t *export_uuid)
{
    lum_export_t *export;

    for (export = lum_exports; export != NULL; export = export->next)
        if (uuid_is_equal(export_get_uuid(export->desc), export_uuid))
            return export;

    return NULL;
}

int lum_export_set_readahead(const exa_uuid_t *export_uuid, uint32_t readahead)
{
    lum_export_t *export;

    EXA_ASSERT(export_uuid != NULL);

    export = lum_export_find_by_uuid(export_uuid);
    if (export == NULL)
        return -VRT_ERR_VOLUME_NOT_EXPORTED;

    if (export_get_type(export->desc) != EXPORT_BDEV)
        return -EXA_ERR_EXPORT_WRONG_METHOD;

    return export->target_adapter->set_readahead(export, readahead);
}

int lum_export_get_nth_connected_iqn(const exa_uuid_t *export_uuid,
                                     unsigned int idx, iqn_t *iqn)
{
    lum_export_t *export;

    EXA_ASSERT(export_uuid != NULL);

    export = lum_export_find_by_uuid(export_uuid);
    if (export == NULL || export_get_type(export->desc) != EXPORT_ISCSI)
        return -VRT_ERR_VOLUME_NOT_EXPORTED;

    return target_get_nth_connected_iqn(export_iscsi_get_lun(export->desc), idx, iqn);
}

const exa_uuid_t *lum_export_get_uuid(const lum_export_t *export)
{
    return export_get_uuid(export->desc);
}

int lum_export_export(const char *buf, size_t buf_size)
{
    exa_uuid_t uuid;
    lum_export_t *lum_export;
    uint64_t sector_count;
    export_type_t type;
    int err;

    lum_export = os_malloc(sizeof(lum_export_t));
    if (lum_export == NULL)
        return -ENOMEM;

    lum_export->desc = export_new();
    if (lum_export->desc == NULL)
    {
        os_free(lum_export);
        return -ENOMEM;
    }

    err = export_deserialize(lum_export->desc, buf, buf_size);
    if (err < 0)
    {
        /* Can't use export_delete() to deallocate since lum_export->desc
           has been allocated with export_new() and the deserialization
           failed, which means it is uninitialized */
        os_free(lum_export->desc);
        os_free(lum_export);
        return err;
    }

    uuid_copy(&uuid, lum_export_get_uuid(lum_export));

    if (lum_export_find_by_uuid(&uuid) != NULL)
    {
        export_delete(lum_export->desc);
        os_free(lum_export);
        return -VRT_ERR_VOLUME_ALREADY_EXPORTED;
    }

    lum_export->blockdevice = vrt_open_volume(&uuid, lum_export_is_readonly(lum_export)
                                              ? BLOCKDEVICE_ACCESS_READ
                                              : BLOCKDEVICE_ACCESS_RW);

    if (lum_export->blockdevice == NULL)
    {
        export_delete(lum_export->desc);
        os_free(lum_export);
        exalog_error("Failed opening volume " UUID_FMT ".", UUID_VAL(&uuid));
        return -ENOMEM;
    }

    sector_count = blockdevice_get_sector_count(lum_export->blockdevice);

    lum_export_add(lum_export);

    type = export_get_type(lum_export->desc);
    EXA_ASSERT_VERBOSE(EXPORT_TYPE_IS_VALID(type), "Unknown export type %i", type);

    switch (type)
    {
    case EXPORT_ISCSI:
        lum_export->target_adapter = get_iscsi_adapter();
        break;
    case EXPORT_BDEV:
#ifdef WITH_BDEV
        lum_export->target_adapter = get_bdev_adapter();
#else
        EXA_ASSERT_VERBOSE(false, "Bdev support is not available");
#endif
        break;
    }

    lum_export->target_adapter->signal_new_export(lum_export, sector_count);

    return EXA_SUCCESS;
}

/* FIXME When -ENOENT, return an export-specific error code instead */
int lum_export_unexport(const exa_uuid_t *uuid)
{
    int err;
    lum_export_t *export;

    EXA_ASSERT(uuid != NULL);

    /* instead of asking the adapter for the export, we should have a local list */
    export = lum_export_find_by_uuid(uuid);
    if (export == NULL)
        return -VRT_ERR_VOLUME_NOT_EXPORTED;

    err = export->target_adapter->signal_remove_export(export);
    if (err != EXA_SUCCESS)
        return err;

    err = vrt_close_volume(export->blockdevice);
    if (err != EXA_SUCCESS)
        return err;

    lum_export_remove(export);

    export_delete(export->desc);
    os_free(export);

    return EXA_SUCCESS;
}

int lum_export_update_iqn_filters(const char *buf, size_t buf_size)
{
    lum_export_t *lum_export;
    export_t *new_export = export_new();
    int err;

    /* Deserialize the new export description into the current LUM export */
    err = export_deserialize(new_export, buf, buf_size);
    if (err < 0)
    {
        /* Can't use export_delete() here since new_export has been
           allocated with export_new() and deserialization failed, which
           means it is uninitialized */
        os_free(new_export);
        return err;
    }

    lum_export = lum_export_find_by_uuid(export_get_uuid(new_export));
    if (lum_export == NULL)
    {
        export_delete(new_export);
        return -VRT_ERR_VOLUME_NOT_EXPORTED;
    }

    if (export_get_type(lum_export->desc) != EXPORT_ISCSI)
    {
        export_delete(new_export);
        return -EXA_ERR_EXPORT_WRONG_METHOD;
    }

    export_iscsi_copy_iqn_filters(lum_export->desc, new_export);

    export_delete(new_export);

    lum_export->target_adapter->signal_export_update_iqn_filters(lum_export);

    return EXA_SUCCESS;
}

int lum_export_resize(const exa_uuid_t *uuid, uint64_t newsizeKB)
{
    lum_export_t *export;
    uint64_t newsize_sectors = newsizeKB * 1024 / SECTOR_SIZE;

    export = lum_export_find_by_uuid(uuid);
    if (export == NULL)
        return -VRT_ERR_VOLUME_NOT_EXPORTED;

    export->target_adapter->export_set_size(export, newsize_sectors);

    return EXA_SUCCESS;
}

int lum_export_get_info(const exa_uuid_t *export_uuid,
                        lum_export_info_reply_t *info)
{
    lum_export_t *export;

    EXA_ASSERT(export_uuid != NULL);
    EXA_ASSERT(info != NULL);

    export = lum_export_find_by_uuid(export_uuid);
    if (export == NULL)
        return -VRT_ERR_VOLUME_NOT_EXPORTED;

    info->in_use   = export->target_adapter->export_get_inuse(export) == EXPORT_IN_USE;
    info->readonly = lum_export_is_readonly(export);

    return EXA_SUCCESS;
}

bool lum_export_is_readonly(const lum_export_t *export)
{
    return export_is_readonly(export->desc);
}

blockdevice_t *lum_export_get_blockdevice(lum_export_t *export)
{
    return export->blockdevice;
}

export_t *lum_export_get_desc(const lum_export_t *export)
{
    return export->desc;
}
