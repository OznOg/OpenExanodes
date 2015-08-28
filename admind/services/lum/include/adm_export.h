/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef ADM_EXPORT_H
#define ADM_EXPORT_H

#include "lum/export/include/export.h"

#include "target/iscsi/include/lun.h"
#include "target/iscsi/include/iqn_filter.h"

#include "common/include/exa_constants.h"
#include "common/include/uuid.h"

#include "os/include/os_inttypes.h"

typedef struct
{
    export_t *desc;
    bool published;
} adm_export_t;

adm_export_t *adm_export_alloc(void);

void adm_export_set(adm_export_t *adm_export, export_t *desc,
                    bool published);

void __adm_export_free(adm_export_t *adm_export);
#define adm_export_free(adm_export) \
    (__adm_export_free(adm_export), adm_export = NULL)

export_type_t adm_export_get_type(const adm_export_t *adm_export);

const exa_uuid_t *adm_export_get_uuid(const adm_export_t *adm_export);

void adm_export_set_published(adm_export_t *adm_export, bool published);
bool adm_export_get_published(adm_export_t *adm_export);

int adm_export_set_lun(adm_export_t *adm_export, lun_t lun);
lun_t adm_export_get_lun(const adm_export_t *adm_export);

const char *adm_export_get_path(const adm_export_t *adm_export);

bool adm_export_is_equal(const adm_export_t *adm_export1,
                         const adm_export_t *adm_export2);

size_t adm_export_serialized_size(void);

int adm_export_serialize(const adm_export_t *adm_export, void *buf, size_t size);

int adm_export_deserialize(adm_export_t *adm_export, const void *buf, size_t size);

#endif /* ADM_EXPORT_H */
