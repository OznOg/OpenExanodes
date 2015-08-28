/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "lum/export/include/export.h"
#include "target/iscsi/include/iqn.h"
#include "target/iscsi/include/iqn_filter.h"
#include "os/include/strlcpy.h"
#include "os/include/os_mem.h"
#include "os/include/os_error.h"
#include "common/include/exa_assert.h"

#include <string.h>

typedef struct
{
    char path[EXA_MAXSIZE_DEVPATH + 1];
} export_bdev_data_t;

typedef struct
{
    lun_t lun;
    iqn_filter_policy_t filter_policy;
    iqn_filter_t iqn_filters[EXA_MAXSIZE_IQN_FILTER_LIST];
    int num_iqn_filters;
} export_iscsi_data_t;

struct export
{
    exa_uuid_t uuid;
    export_type_t type;
    bool readonly;
    union
    {
        export_bdev_data_t *bdev;
        export_iscsi_data_t *iscsi;
    } info;
};

typedef struct
{
    export_type_t type;
    exa_uuid_t uuid;
    bool readonly;
    union
    {
        export_bdev_data_t bdev;
        export_iscsi_data_t iscsi;
    };
} serialized_export_t;

export_t *export_new(void)
{
    return os_malloc(sizeof(export_t));
}

/* XXX Replace with export_set(), to be used with export_new()? */
/**
 * Allocate an export with the specified type and UUID.
 *
 * @param[in] type           Type of export
 * @param[in] uuid           Export UUID
 *
 * @return export if successfully allocated, NULL otherwise
 */
static export_t *export_new2(export_type_t type, const exa_uuid_t *uuid)
{
    export_t *export;

    if (!EXPORT_TYPE_IS_VALID(type))
        return NULL;
    if (uuid == NULL)
        return NULL;

    export = export_new();
    if (export == NULL)
        return NULL;

    export->type     = type;
    /* By default export is RW */
    export->readonly = false;

    export->info.bdev = NULL;
    export->info.iscsi = NULL;

    uuid_copy(&export->uuid, uuid);

    return export;
}

static void export_bdev_data_delete(export_bdev_data_t *bdev)
{
    if (bdev == NULL)
        return;

    os_free(bdev);
}

static void export_iscsi_data_delete(export_iscsi_data_t *iscsi)
{
    if (iscsi == NULL)
        return;

    os_free(iscsi);
}

export_t *export_new_bdev(const exa_uuid_t *uuid, const char *path)
{
    export_t *export;
    export_bdev_data_t *bdev;

    if (path == NULL)
        return NULL;

    if (strlen(path) > EXA_MAXSIZE_DEVPATH)
        return NULL;

    bdev = os_malloc(sizeof(export_bdev_data_t));
    if (bdev == NULL)
        return NULL;

    os_strlcpy(bdev->path, path, sizeof(bdev->path));

    export = export_new2(EXPORT_BDEV, uuid);
    if (export == NULL)
    {
        export_bdev_data_delete(bdev);
        return NULL;
    }
    export->info.bdev = bdev;

    return export;
}

export_t *export_new_iscsi(const exa_uuid_t *uuid, lun_t lun,
                           iqn_filter_policy_t filter_policy)
{
    export_t *export;
    export_iscsi_data_t *iscsi;

    if (!LUN_IS_VALID(lun))
        return NULL;

    if (!IQN_FILTER_POLICY_IS_VALID(filter_policy))
        return NULL;

    iscsi = os_malloc(sizeof(export_iscsi_data_t));
    if (iscsi == NULL)
        return NULL;

    iscsi->lun = lun;
    iscsi->filter_policy = filter_policy;
    memset(iscsi->iqn_filters, 0, sizeof(iscsi->iqn_filters));
    iscsi->num_iqn_filters = 0;

    export = export_new2(EXPORT_ISCSI, uuid);
    if (export == NULL)
    {
        export_iscsi_data_delete(iscsi);
        return NULL;
    }
    export->info.iscsi = iscsi;

    return export;
}

void export_delete(export_t *export)
{
    if (export == NULL)
        return;

    EXA_ASSERT_VERBOSE(EXPORT_TYPE_IS_VALID(export->type),
                       "Invalid export type: %d", export->type);

    switch (export->type)
    {
        case EXPORT_BDEV:
            export_bdev_data_delete(export->info.bdev);
            break;
        case EXPORT_ISCSI:
            export_iscsi_data_delete(export->info.iscsi);
            break;
    }

    os_free(export);
}

static bool export_bdev_data_is_equal(const export_bdev_data_t *d1,
                                      const export_bdev_data_t *d2)
{
    return strcmp(d1->path, d2->path) == 0;
}

static bool export_iscsi_data_is_equal(const export_iscsi_data_t *d1,
                                       const export_iscsi_data_t *d2)
{
    int i;

    if (d1->lun != d2->lun)
        return false;

    if (d1->filter_policy != d2->filter_policy)
        return false;

    if (d1->num_iqn_filters != d2->num_iqn_filters)
        return false;

    for (i = 0; i < d1->num_iqn_filters; i++)
        if (!iqn_filter_is_equal(&d1->iqn_filters[i],
                                 &d2->iqn_filters[i]))
            return false;

    return true;
}

bool export_is_equal(const export_t *export1, const export_t *export2)
{
    EXA_ASSERT(export1 != NULL);
    EXA_ASSERT(export2 != NULL);

    if (export1->type != export2->type)
        return false;

    if (!uuid_is_equal(&export1->uuid, &export2->uuid))
        return false;

    if (export1->readonly != export2->readonly)
        return false;

    switch (export1->type)
    {
    case EXPORT_BDEV:
        if (!export_bdev_data_is_equal(export1->info.bdev,
                                       export2->info.bdev))
            return false;
        break;

    case EXPORT_ISCSI:
        if (!export_iscsi_data_is_equal(export1->info.iscsi,
                                        export2->info.iscsi))
            return false;
        break;
    }

    return true;
}

static void export_iscsi_data_copy(export_iscsi_data_t *dest,
                                   const export_iscsi_data_t *src)
{
    unsigned int i;

    dest->lun = src->lun;
    dest->filter_policy = src->filter_policy;
    dest->num_iqn_filters = src->num_iqn_filters;

    for (i = 0; i < dest->num_iqn_filters; i++)
        iqn_filter_copy(&dest->iqn_filters[i], &src->iqn_filters[i]);
}

void export_iscsi_copy_iqn_filters(export_t *dest, const export_t *src)
{
    EXA_ASSERT(dest != NULL && dest->type == EXPORT_ISCSI);
    EXA_ASSERT(src != NULL && src->type == EXPORT_ISCSI);

    export_iscsi_data_copy(dest->info.iscsi, src->info.iscsi);
}

size_t export_serialized_size(void)
{
    return sizeof(serialized_export_t);
}

int export_serialize(const export_t *export, void *buf, size_t size)
{
    serialized_export_t *ser;

    if (export == NULL || buf == NULL || size < sizeof(serialized_export_t))
        return -EINVAL;

    EXA_ASSERT(EXPORT_TYPE_IS_VALID(export->type));

    ser = buf;

    ser->type = export->type;
    uuid_copy(&ser->uuid, &export->uuid);
    ser->readonly = export->readonly;

    switch (export->type)
    {
    case EXPORT_BDEV:
        memcpy(&ser->bdev, export->info.bdev, sizeof(ser->bdev));
        break;

    case EXPORT_ISCSI:
        memcpy(&ser->iscsi, export->info.iscsi, sizeof(ser->iscsi));
        break;
    }

    return sizeof(serialized_export_t);
}

int export_deserialize(export_t *export, const void *buf, size_t size)
{
    const serialized_export_t *ser;

    if (export == NULL || buf == NULL || size < sizeof(serialized_export_t))
        return -EINVAL;

    ser = buf;

    EXA_ASSERT(EXPORT_TYPE_IS_VALID(ser->type));

    export->type = ser->type;
    uuid_copy(&export->uuid, &ser->uuid);
    export->readonly = ser->readonly;

    switch (export->type)
    {
    case EXPORT_BDEV:
        export->info.bdev = os_malloc(sizeof(export_bdev_data_t));
        if (export->info.bdev == NULL)
            return -ENOMEM;
        memcpy(export->info.bdev, &ser->bdev, sizeof(*export->info.bdev));
        break;

    case EXPORT_ISCSI:
        export->info.iscsi = os_malloc(sizeof(export_iscsi_data_t));
        if (export->info.iscsi == NULL)
            return -ENOMEM;
        memcpy(export->info.iscsi, &ser->iscsi, sizeof(*export->info.iscsi));
        break;
    }

    return sizeof(serialized_export_t);
}

export_type_t export_get_type(const export_t *export)
{
    if (export == NULL)
        return EXPORT_TYPE__INVALID;
    return export->type;
}

const exa_uuid_t *export_get_uuid(const export_t *export)
{
    if (export == NULL)
        return NULL;
    return &export->uuid;
}

bool export_is_readonly(const export_t *export)
{
  return export->readonly;
}

void export_set_readonly(export_t *export, bool readonly)
{
    export->readonly = readonly;
}

const char *export_bdev_get_path(const export_t *export)
{
    if (export == NULL)
        return NULL;
    if (export_get_type(export) != EXPORT_BDEV)
        return NULL;

    return export->info.bdev->path;
}

lun_t export_iscsi_get_lun(const export_t *export)
{
    if (export == NULL)
        return LUN_NONE;
    if (export_get_type(export) != EXPORT_ISCSI)
        return LUN_NONE;

    return export->info.iscsi->lun;
}

exa_error_code export_iscsi_set_lun(export_t *export, lun_t new_lun)
{
    if (export == NULL)
        return -LUM_ERR_INVALID_EXPORT;
    if (export_get_type(export) != EXPORT_ISCSI)
        return -LUM_ERR_INVALID_EXPORT;
    if (!LUN_IS_VALID(new_lun))
        return -LUM_ERR_INVALID_LUN;

    export->info.iscsi->lun = new_lun;

    return EXA_SUCCESS;
}

iqn_filter_policy_t export_iscsi_get_filter_policy(const export_t *export)
{
    if (export == NULL)
        return IQN_FILTER_NONE;
    if (export_get_type(export) != EXPORT_ISCSI)
        return IQN_FILTER_NONE;

    return export->info.iscsi->filter_policy;
}

exa_error_code export_iscsi_set_filter_policy(export_t *export,
                                              iqn_filter_policy_t new_policy)
{
    if (export == NULL)
        return -LUM_ERR_INVALID_EXPORT;
    if (export_get_type(export) != EXPORT_ISCSI)
        return -LUM_ERR_INVALID_EXPORT;
    if (!IQN_FILTER_POLICY_IS_VALID(new_policy))
        return -LUM_ERR_INVALID_IQN_FILTER_POLICY;

    export->info.iscsi->filter_policy = new_policy;

    return EXA_SUCCESS;
}

/**
 * Find if the given IQN pattern matches one of the IQN filters we have.
 *
 * This function performs an exact match and is used internally to avoid
 * adding the same filter multiple times, *not* to help determining the
 * filtering policy for a given IQN.
 *
 * @param[in] export       The export we're looking in
 * @param[in] iqn_pattern  The IQN pattern we're trying to match
 *
 * @return The index of the first matching filter greater than or equal to
 *         zero, -EXA_ERR_NOT_FOUND if nothing matched, -EXA_ERR_INVALID_PARAM
 *         if the export or the iqn is NULL.
 */
static int export_iscsi_find_iqn_filter(const export_t *export, const iqn_t *iqn_pattern)
{
    export_iscsi_data_t *iscsi;
    int i;

    if (export == NULL)
        return -EXA_ERR_INVALID_PARAM;

    iscsi = export->info.iscsi;

    for (i = 0; i < iscsi->num_iqn_filters; i++)
    {
        const iqn_t *filter_pattern = iqn_filter_get_pattern(&iscsi->iqn_filters[i]);
        if (iqn_is_equal(iqn_pattern, filter_pattern))
            return i;
    }

    return -EXA_ERR_NOT_FOUND;
}

exa_error_code export_iscsi_add_iqn_filter(export_t *export,
                                           const iqn_t *iqn_pattern,
                                           iqn_filter_policy_t policy)
{
    int i;

    if (export == NULL)
        return -LUM_ERR_INVALID_EXPORT;

    if (iqn_pattern == NULL)
        return -LUM_ERR_INVALID_IQN_FILTER;

    if (export_get_type(export) != EXPORT_ISCSI)
        return -LUM_ERR_INVALID_EXPORT;

    if (!IQN_FILTER_POLICY_IS_VALID(policy))
        return -LUM_ERR_INVALID_IQN_FILTER_POLICY;

    i = export->info.iscsi->num_iqn_filters;
    if (i >= EXA_MAXSIZE_IQN_FILTER_LIST)
        return -LUM_ERR_TOO_MANY_IQN_FILTERS;

    if (export_iscsi_find_iqn_filter(export, iqn_pattern) >= 0)
        return -LUM_ERR_DUPLICATE_IQN_FILTER;

    iqn_filter_set(&export->info.iscsi->iqn_filters[i], iqn_pattern, policy);

    export->info.iscsi->num_iqn_filters++;

    return EXA_SUCCESS;
}

iqn_filter_policy_t export_iscsi_get_policy_for_iqn(const export_t *export,
                                                    const iqn_t *iqn)
{
    export_iscsi_data_t *iscsi;
    int i;

    if (export == NULL || iqn == NULL)
        return IQN_FILTER_NONE;

    if (export_get_type(export) != EXPORT_ISCSI)
        return IQN_FILTER_NONE;

    iscsi = export->info.iscsi;
    for (i = 0; i < iscsi->num_iqn_filters; i++)
    {
        iqn_filter_policy_t policy;

        if (iqn_filter_matches(&iscsi->iqn_filters[i], iqn, &policy))
            return policy;
    }

    /* Return the default policy */
    return export_iscsi_get_filter_policy(export);
}

int export_iscsi_get_iqn_filters_number(const export_t *export)
{
    if (export == NULL)
        return EXPORT_INVALID_PARAM;
    if (export_get_type(export) != EXPORT_ISCSI)
        return EXPORT_INVALID_PARAM;

    return export->info.iscsi->num_iqn_filters;
}

exa_error_code export_iscsi_remove_iqn_filter(export_t *export,
                                              const iqn_t *iqn_pattern)
{
    export_iscsi_data_t *iscsi;
    int i, n;

    if (export == NULL)
        return -LUM_ERR_INVALID_EXPORT;

    if (iqn_pattern == NULL)
        return -LUM_ERR_INVALID_IQN_FILTER;

    if (export_get_type(export) != EXPORT_ISCSI)
        return -LUM_ERR_INVALID_EXPORT;

    n = export_iscsi_find_iqn_filter(export, iqn_pattern);
    if (n == -EXA_ERR_NOT_FOUND)
        return -LUM_ERR_IQN_FILTER_NOT_FOUND;
    if (n < 0)
        return n;

    iscsi = export->info.iscsi;

    iscsi->num_iqn_filters--;

    for (i = n; i < iscsi->num_iqn_filters; i++)
        iqn_filter_copy(&iscsi->iqn_filters[i], &iscsi->iqn_filters[i + 1]);

    return EXA_SUCCESS;
}

exa_error_code export_iscsi_clear_iqn_filters(export_t *export)
{
    if (export == NULL)
        return -LUM_ERR_INVALID_EXPORT;

    if (export_get_type(export) != EXPORT_ISCSI)
        return -LUM_ERR_INVALID_EXPORT;

    export->info.iscsi->num_iqn_filters = 0;

    return EXA_SUCCESS;
}

exa_error_code export_iscsi_clear_iqn_filters_policy(export_t *export,
                                                     iqn_filter_policy_t policy)
{
    int i;
    if (export == NULL)
        return -LUM_ERR_INVALID_EXPORT;
    if (export_get_type(export) != EXPORT_ISCSI)
        return -LUM_ERR_INVALID_EXPORT;

    i = 0;
    while (i < export->info.iscsi->num_iqn_filters)
    {
        const iqn_filter_t *filter = export_iscsi_get_nth_iqn_filter(export, i);

        if (filter != NULL && iqn_filter_get_policy(filter) == policy)
            export_iscsi_remove_iqn_filter(export, iqn_filter_get_pattern(filter));
        else
            i++;
    }

    return EXA_SUCCESS;
}

const iqn_filter_t *export_iscsi_get_nth_iqn_filter(const export_t *export, int n)
{
    if (export == NULL)
        return NULL;

    if (export_get_type(export) != EXPORT_ISCSI)
        return NULL;

    if (n < 0 || n >= export->info.iscsi->num_iqn_filters)
        return NULL;

    return &export->info.iscsi->iqn_filters[n];
}

void export_get_info(const export_t *export, export_info_t *info)
{
    EXA_ASSERT(export != NULL && info != NULL);

    uuid_copy(&info->uuid, &export->uuid);
    info->type = export->type;

    EXA_ASSERT_VERBOSE(EXPORT_TYPE_IS_VALID(export->type),
		       "Invalid export type value");

    switch (export->type)
    {
    case EXPORT_BDEV:
	strlcpy(info->bdev.path, export->info.bdev->path, EXA_MAXSIZE_DEVPATH + 1);
	break;
    case EXPORT_ISCSI:
	info->iscsi.lun = export->info.iscsi->lun;
	break;
    }
}
