/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/lum/include/adm_export.h"
#include "lum/export/include/export.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"

#include "common/include/exa_assert.h"

#include <errno.h>

adm_export_t *adm_export_alloc(void)
{
    return os_malloc(sizeof(adm_export_t));
}

void adm_export_set(adm_export_t *adm_export, export_t *desc, bool published)
{
    if (adm_export == NULL || desc == NULL)
        return;

    adm_export->desc = desc;
    adm_export->published = published;
}

void __adm_export_free(adm_export_t *adm_export)
{
    if (adm_export == NULL)
        return;

    export_delete(adm_export->desc);
    os_free(adm_export);
}

export_type_t adm_export_get_type(const adm_export_t *adm_export)
{
    EXA_ASSERT(adm_export != NULL);

    return export_get_type(adm_export->desc);
}

const exa_uuid_t *adm_export_get_uuid(const adm_export_t *adm_export)
{
    if (adm_export == NULL)
        return NULL;

    return export_get_uuid(adm_export->desc);
}

void adm_export_set_published(adm_export_t *adm_export, bool published)
{
    EXA_ASSERT(adm_export != NULL);
    adm_export->published = published;
}

bool adm_export_get_published(adm_export_t *adm_export)
{
    EXA_ASSERT(adm_export != NULL);
    return adm_export->published;
}

int adm_export_set_lun(adm_export_t *adm_export, lun_t lun)
{
    EXA_ASSERT(adm_export != NULL);
    EXA_ASSERT(LUN_IS_VALID(lun));

    return export_iscsi_set_lun(adm_export->desc, lun);
}

lun_t adm_export_get_lun(const adm_export_t *adm_export)
{
    if (adm_export == NULL)
        return LUN_NONE;

    EXA_ASSERT(export_get_type(adm_export->desc) == EXPORT_ISCSI);

    return export_iscsi_get_lun(adm_export->desc);
}

const char *adm_export_get_path(const adm_export_t *adm_export)
{
    if (adm_export == NULL)
        return NULL;

    EXA_ASSERT(export_get_type(adm_export->desc) == EXPORT_BDEV);

    return export_bdev_get_path(adm_export->desc);
}

bool adm_export_is_equal(const adm_export_t *adm_export1,
                         const adm_export_t *adm_export2)
{
    EXA_ASSERT(adm_export1 != NULL);
    EXA_ASSERT(adm_export2 != NULL);

    return export_is_equal(adm_export1->desc, adm_export2->desc)
           && adm_export1->published == adm_export2->published;
}

size_t adm_export_serialized_size(void)
{
    return export_serialized_size() + sizeof(bool);
}

int adm_export_serialize(const adm_export_t *adm_export, void *buf, size_t size)
{
    int written;

    if (adm_export == NULL || buf == NULL || size < adm_export_serialized_size())
        return -EINVAL;

    written = export_serialize(adm_export->desc, buf, size);
    if (written < 0)
        return written;

    memcpy((char *)buf + written, &adm_export->published,
           sizeof(adm_export->published));
    written += sizeof(adm_export->published);

    return written;
}

int adm_export_deserialize(adm_export_t *adm_export, const void *buf, size_t size)
{
    int _read;

    if (adm_export == NULL || buf == NULL || size < adm_export_serialized_size())
        return -EINVAL;

    adm_export->desc = export_new();
    if (adm_export->desc == NULL)
        return -ENOMEM;

    _read = export_deserialize(adm_export->desc, buf, size);
    if (_read < 0)
    {
        os_free(adm_export->desc);
        return _read;
    }

    memcpy(&adm_export->published, (char *)buf + _read,
           sizeof(adm_export->published));
    _read += sizeof(adm_export->published);

    return _read;
}
