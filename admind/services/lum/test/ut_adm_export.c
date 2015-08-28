/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "admind/services/lum/include/adm_export.h"
#include "lum/export/include/export.h"
#include "common/include/uuid.h"

/* FIXME Missing lots of test cases! */

UT_SECTION(serialization)

static bool __serialization_ok(const adm_export_t *adm_export)
{
    size_t buf_size = adm_export_serialized_size();
    char buf[buf_size];
    adm_export_t *adm_export2;
    bool ok;

    adm_export2 = adm_export_alloc();
    UT_ASSERT(adm_export2 != NULL);

    UT_ASSERT_EQUAL(buf_size, adm_export_serialize(adm_export, buf, buf_size));
    UT_ASSERT_EQUAL(buf_size, adm_export_deserialize(adm_export2, buf, buf_size));

    ok = adm_export_is_equal(adm_export2, adm_export);

    adm_export_free(adm_export2);

    return ok;
}

ut_test(serialize_deserialize_is_identity)
{
    adm_export_t *adm_export;
    export_t *desc;
    exa_uuid_t uuid;
    /* lun_t lun; */
    /* iqn_filter_policy_t policy; */

    uuid_scan("12345678:12345678:12345678:12345678", &uuid);

    adm_export = adm_export_alloc();
    UT_ASSERT(adm_export != NULL);

    desc = export_new_bdev(&uuid, "/some/arbitrary/path");
    UT_ASSERT(desc != NULL);
    adm_export_set(adm_export, desc, false);

    UT_ASSERT(__serialization_ok(adm_export));

    adm_export_free(adm_export);
}
