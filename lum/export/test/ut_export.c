/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "lum/export/include/export.h"
#include "os/include/os_stdio.h"

#include <string.h>

#define EXPORT_UUID  "20524188:2A2F4B01:52D56CF5:28017CA9"
#define IQN_STR      "iqn.2010-06.com.seanodes:ut_test_iqn"
#define BDEV_PATH    "/dev/sdb"
#define LUN          2
#define NEG_LUN      (-1)
#define BIG_LUN      (MAX_LUNS + 1)

static iqn_t g_iqn;
static iqn_t g_non_existent_iqn;

UT_SECTION(export_new)

ut_setup()
{
    UT_ASSERT(iqn_from_str(&g_iqn, IQN_STR) == 0);
    UT_ASSERT(iqn_from_str(&g_non_existent_iqn, "iqn.2012-21.org.hahaha:non_existent_iqn") == 0);
}

ut_cleanup()
{
}

ut_test(export_delete_null)
{
    export_delete(NULL);
}

ut_test(export_new_bdev_return_correct_export)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);
    UT_ASSERT_EQUAL(export_get_type(export), EXPORT_BDEV);
    UT_ASSERT(uuid_is_equal(export_get_uuid(export), &uuid));
    UT_ASSERT_EQUAL(0, strcmp(export_bdev_get_path(export), BDEV_PATH));

    export_delete(export);
}

ut_test(export_new_iscsi_return_correct_export)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);
    UT_ASSERT_EQUAL(EXPORT_ISCSI, export_get_type(export));
    UT_ASSERT(uuid_is_equal(export_get_uuid(export), &uuid));
    UT_ASSERT(!export_is_readonly(export));
    UT_ASSERT_EQUAL(LUN, export_iscsi_get_lun(export));
    UT_ASSERT_EQUAL(IQN_FILTER_REJECT, export_iscsi_get_filter_policy(export));

    export_delete(export);
}

ut_test(readonly_set_and_get)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);

    UT_ASSERT(export != NULL);
    UT_ASSERT(!export_is_readonly(export));
    export_set_readonly(export, false);
    UT_ASSERT(!export_is_readonly(export));
    export_set_readonly(export, true);
    UT_ASSERT(export_is_readonly(export));
    export_set_readonly(export, false);
    UT_ASSERT(!export_is_readonly(export));
}

ut_test(export_new_bdev_NULL_path_return_NULL)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, NULL);
    UT_ASSERT(export == NULL);
}

ut_test(export_new_bdev_too_long_path_return_NULL)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char long_path[EXA_MAXSIZE_DEVPATH + 2];

    uuid_scan(EXPORT_UUID, &uuid);
    memset(long_path, '/', EXA_MAXSIZE_DEVPATH + 1);
    long_path[EXA_MAXSIZE_DEVPATH + 1] = '\0';
    export = export_new_bdev(&uuid, long_path);
    UT_ASSERT(export == NULL);
}

ut_test(export_new_iscsi_neg_lun_return_NULL)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, NEG_LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export == NULL);
}

ut_test(export_new_iscsi_big_lun_return_NULL)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, BIG_LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export == NULL);
}

ut_test(export_new_iscsi_invalid_filter_policy_return_NULL)
{
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);
    UT_ASSERT(export_new_iscsi(&uuid, LUN, IQN_FILTER_POLICY__FIRST - 1) == NULL);
    UT_ASSERT(export_new_iscsi(&uuid, LUN, IQN_FILTER_POLICY__LAST + 1) == NULL);
}

UT_SECTION(export_is_equal)

ut_test(exports_of_different_types_are_not_equal)
{
    export_t *e1, *e2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_bdev(&uuid, "a/fictitious/path");
    e2 = export_new_iscsi(&uuid, 23, IQN_FILTER_REJECT);

    UT_ASSERT(!export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(exports_with_different_uuids_are_not_equal)
{
    export_t *e1, *e2;
    exa_uuid_t uuid;

    uuid_scan("11111111:22222222:33333333:44444444", &uuid);
    e1 = export_new_bdev(&uuid, "a/fictitious/path");

    uuid_scan("12345678:12345678:12345678:12345678", &uuid);
    e2 = export_new_bdev(&uuid, "a/fictitious/path");

    UT_ASSERT(!export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(exports_with_different_readonly_are_not_equal)
{
    export_t *e1, *e2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_bdev(&uuid, "a/fictitious/path");
    export_set_readonly(e1, false);
    e2 = export_new_bdev(&uuid, "a/fictitious/path");
    export_set_readonly(e1, true);

    UT_ASSERT(!export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(bdev_exports_with_different_paths_are_not_equal)
{
    export_t *e1, *e2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_bdev(&uuid, "a/fictitious/path");
    e2 = export_new_bdev(&uuid, "a/different/path");

    UT_ASSERT(!export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(iscsi_exports_with_different_luns_are_not_equal)
{
    export_t *e1, *e2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_iscsi(&uuid, 31, IQN_FILTER_ACCEPT);
    e2 = export_new_iscsi(&uuid, 75, IQN_FILTER_ACCEPT);

    UT_ASSERT(!export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(iscsi_exports_with_different_iqn_filters_are_not_equal)
{
    export_t *e1, *e2;
    iqn_t iqn1, iqn2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_iscsi(&uuid, 31, IQN_FILTER_ACCEPT);
    iqn_from_str(&iqn1, IQN_STR);
    export_iscsi_add_iqn_filter(e1, &iqn1, IQN_FILTER_REJECT);

    e2 = export_new_iscsi(&uuid, 75, IQN_FILTER_ACCEPT);
    iqn_from_str(&iqn2, "iqn.2010-06.org.tralala:gruik_pouet_coin");
    export_iscsi_add_iqn_filter(e2, &iqn2, IQN_FILTER_REJECT);

    UT_ASSERT(!export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(same_bdev_exports_are_equal)
{
    export_t *e1, *e2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_bdev(&uuid, "same/path/for/both/exports");
    export_set_readonly(e1, true);
    e2 = export_new_bdev(&uuid, "same/path/for/both/exports");
    export_set_readonly(e2, true);

    UT_ASSERT(export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

ut_test(same_iscsi_exports_are_equal)
{
    export_t *e1, *e2;
    iqn_t iqn1, iqn2;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);

    e1 = export_new_iscsi(&uuid, 31, IQN_FILTER_ACCEPT);
    export_set_readonly(e1, true);
    iqn_from_str(&iqn1, IQN_STR);
    export_iscsi_add_iqn_filter(e1, &iqn1, IQN_FILTER_REJECT);

    e2 = export_new_iscsi(&uuid, 31, IQN_FILTER_ACCEPT);
    export_set_readonly(e2, true);
    iqn_from_str(&iqn2, IQN_STR);
    export_iscsi_add_iqn_filter(e2, &iqn2, IQN_FILTER_REJECT);

    UT_ASSERT(export_is_equal(e1, e2));

    export_delete(e1);
    export_delete(e2);
}

UT_SECTION(serialization)

ut_test(serialize_null_export_returns_EINVAL)
{
    size_t buf_size = export_serialized_size();
    char buf[buf_size];

    UT_ASSERT_EQUAL(-EINVAL, export_serialize(NULL, buf, buf_size));
}

ut_test(serialize_to_null_buffer_returns_EINVAL)
{
    size_t buf_size = export_serialized_size();
    export_t *export;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, "blah/blah/blah");

    UT_ASSERT_EQUAL(-EINVAL, export_serialize(export, NULL, buf_size));
}

ut_test(serialize_to_too_small_a_buffer_returns_EINVAL)
{
    size_t buf_size = export_serialized_size() - 1;
    char buf[buf_size];
    export_t *export;
    exa_uuid_t uuid;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, "blah/blah/blah");

    UT_ASSERT_EQUAL(-EINVAL, export_serialize(export, buf, buf_size));
}

static bool __serialization_ok(const export_t *export)
{
    size_t buf_size = export_serialized_size();
    char buf[buf_size];
    export_t *export2;
    bool ok;

    export2 = export_new();
    UT_ASSERT(export2 != NULL);

    UT_ASSERT_EQUAL(buf_size, export_serialize(export, buf, buf_size));
    UT_ASSERT_EQUAL(buf_size, export_deserialize(export2, buf, buf_size));

    ok = export_is_equal(export2, export);

    export_delete(export2);

    return ok;
}

ut_test(serialize_deserialize_is_identity)
{
    export_t *export;
    exa_uuid_t uuid;
    lun_t lun;
    iqn_filter_policy_t policy;
    struct entry {
        const char *pattern_str;
        iqn_filter_policy_t policy;
    } entries[] = {
        { "iqn.2010-06.*:ut_test_iqn", IQN_FILTER_ACCEPT },
        { "iqn.1997-05.*", IQN_FILTER_REJECT },
        { NULL, 0 }
    };
    int i;

    uuid_scan("12345678:12345678:12345678:12345678", &uuid);

    /* bdev */

    export = export_new_bdev(&uuid, "/some/arbitrary/path");
    UT_ASSERT(export != NULL);
    UT_ASSERT(__serialization_ok(export));
    export_delete(export);

    /* iSCSI */

    lun = 247;
    policy = IQN_FILTER_ACCEPT;
    export = export_new_iscsi(&uuid, lun, policy);
    UT_ASSERT(export != NULL);
    for (i = 0; entries[i].pattern_str != NULL; i++)
    {
        iqn_t iqn;

        iqn_from_str(&iqn, entries[i].pattern_str);
        export_iscsi_add_iqn_filter(export, &iqn, entries[i].policy);
    }
    UT_ASSERT(__serialization_ok(export));

    export_delete(export);
}

UT_SECTION(getters)

ut_test(export_get_type_on_null_returns_EXPORT_TYPE__INVALID)
{
    UT_ASSERT(export_get_type(NULL) == EXPORT_TYPE__INVALID);
}

ut_test(export_get_uuid_on_null_returns_NULL)
{
    UT_ASSERT(export_get_uuid(NULL) == NULL);
}

ut_test(export_bdev_get_path_on_null_returns_NULL)
{
    UT_ASSERT(export_bdev_get_path(NULL) == NULL);
}

ut_test(export_iscsi_get_lun_on_null_returns_LUN_NONE)
{
    UT_ASSERT(export_iscsi_get_lun(NULL) == LUN_NONE);
}

ut_test(export_iscsi_get_filter_policy_on_null_returns_IQN_FILTER_NONE)
{
    UT_ASSERT(export_iscsi_get_filter_policy(NULL) == IQN_FILTER_NONE);
}

ut_test(export_bdev_get_path_on_wrong_export_returns_NULL)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);
    UT_ASSERT(export_bdev_get_path(export) == NULL);

    export_delete(export);
}

ut_test(export_iscsi_get_lun_on_wrong_export_returns_LUN_NONE)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);
    UT_ASSERT(export_iscsi_get_lun(export) == LUN_NONE);

    export_delete(export);
}

ut_test(export_iscsi_get_filter_policy_on_wrong_export_returns_IQN_FILTER_NONE)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);
    UT_ASSERT(export_iscsi_get_filter_policy(export) == IQN_FILTER_NONE);

    export_delete(export);
}

UT_SECTION(setters)

ut_test(export_iscsi_set_lun_correct_return_EXA_SUCCESS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_set_lun(export, LUN + 1) == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_get_lun(export) == LUN + 1);

    export_delete(export);
}

ut_test(export_iscsi_set_lun_null_export_return_LUM_ERR_INVALID_EXPORT)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_set_lun(NULL, LUN + 1));
}

ut_test(export_iscsi_set_lun_wrong_export_return_LUM_ERR_INVALID_EXPORT)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_set_lun(export, LUN + 1));

    export_delete(export);
}

ut_test(export_iscsi_set_lun_wrong_lun_return_LUM_ERR_INVALID_LUN)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_LUN, export_iscsi_set_lun(export, -1));
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_LUN, export_iscsi_set_lun(export, MAX_LUNS+1));
    UT_ASSERT(export_iscsi_get_lun(export) == LUN);

    export_delete(export);
}

UT_SECTION(filter_policy)

ut_test(export_iscsi_set_filter_policy_correct_return_EXA_SUCCESS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_set_filter_policy(export, IQN_FILTER_ACCEPT)
              == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_get_filter_policy(export) == IQN_FILTER_ACCEPT);

    export_delete(export);
}

ut_test(export_iscsi_set_filter_policy_null_export_return_LUM_ERR_INVALID_EXPORT)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_set_filter_policy(NULL, LUN + 1));
}

ut_test(export_iscsi_set_filter_policy_wrong_export_return_LUM_ERR_INVALID_EXPORT)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_set_filter_policy(export, IQN_FILTER_ACCEPT));

    export_delete(export);
}

ut_test(export_iscsi_set_filter_policy_wrong_filter_policy_return_LUM_ERR_INVALID_IQN_FILTER_POLICY)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_IQN_FILTER_POLICY, export_iscsi_set_filter_policy(
              export, IQN_FILTER_POLICY__FIRST - 1));
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_IQN_FILTER_POLICY, export_iscsi_set_filter_policy(
              export, IQN_FILTER_POLICY__LAST + 1));
    UT_ASSERT_EQUAL(IQN_FILTER_REJECT, export_iscsi_get_filter_policy(export));

    export_delete(export);
}

UT_SECTION(iqn_filters)

ut_setup()
{
    UT_ASSERT(iqn_from_str(&g_iqn, IQN_STR) == 0);
    UT_ASSERT(iqn_from_str(&g_non_existent_iqn, "iqn.2012-21.org.hahaha:non_existent_iqn") == 0);
}

ut_cleanup()
{
}

ut_test(export_iscsi_add_iqn_filter_correct_return_EXA_SUCCESS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_ACCEPT)
              == EXA_SUCCESS);

    export_delete(export);
}

ut_test(export_iscsi_add_iqn_filter_wrong_export_return_LUM_ERR_INVALID_EXPORT)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_add_iqn_filter(export, &g_iqn,
            IQN_FILTER_ACCEPT));

    export_delete(export);
}

ut_test(export_iscsi_add_iqn_filter_null_export_return_LUM_ERR_INVALID_EXPORT)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT,
                    export_iscsi_add_iqn_filter(NULL, &g_iqn, IQN_FILTER_ACCEPT));
}

ut_test(export_iscsi_add_iqn_filter_null_iqn_return_LUM_ERR_INVALID_IQN_FILTER)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_IQN_FILTER,
                    export_iscsi_add_iqn_filter(export, NULL, IQN_FILTER_ACCEPT));

    export_delete(export);
}

ut_test(export_iscsi_add_iqn_filter_wrong_policy_return_EXA_ERR_INVALID_VALUE)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_IQN_FILTER_POLICY,
                    export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_POLICY__FIRST - 1));
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_IQN_FILTER_POLICY,
                    export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_POLICY__LAST + 1));

    export_delete(export);
}

ut_test(export_iscsi_add_iqn_filter_same_filter_return_LUM_ERR_DUPLICATE_IQN_FILTER)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(EXA_SUCCESS, export_iscsi_add_iqn_filter(export, &g_iqn,
            IQN_FILTER_ACCEPT));
    UT_ASSERT_EQUAL(-LUM_ERR_DUPLICATE_IQN_FILTER, export_iscsi_add_iqn_filter(export, &g_iqn,
            IQN_FILTER_ACCEPT));

    export_delete(export);
}

ut_test(export_iscsi_add_iqn_filter_too_many_return_LUM_ERR_TOO_MANY_IQN_FILTERS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn;
    int i;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    for (i = 0; i < EXA_MAXSIZE_IQN_FILTER_LIST; i++)
    {
        os_snprintf(iqn_str, sizeof(iqn_str), "%s.%d", IQN_STR, i);
        UT_ASSERT(iqn_from_str(&iqn, iqn_str) == 0);

        UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_ACCEPT)
                  == EXA_SUCCESS);
    }

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.%d", IQN_STR, i);
    UT_ASSERT(iqn_from_str(&iqn, iqn_str) == 0);

    UT_ASSERT_EQUAL(-LUM_ERR_TOO_MANY_IQN_FILTERS, export_iscsi_add_iqn_filter(export, &iqn,
            IQN_FILTER_ACCEPT));

    export_delete(export);
}

ut_test(export_iscsi_get_policy_for_iqn_correct_return_correct_policy)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_ACCEPT)
              == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_get_policy_for_iqn(export, &g_iqn)
              == IQN_FILTER_ACCEPT);

    export_delete(export);
}

ut_test(export_iscsi_get_policy_for_not_found_iqn_correct_return_default_policy)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_ACCEPT)
              == EXA_SUCCESS);

    UT_ASSERT(export_iscsi_get_policy_for_iqn(export, &g_non_existent_iqn)
              == IQN_FILTER_REJECT);

    export_delete(export);
}

ut_test(export_iscsi_get_policy_for_wrong_export_return_IQN_FILTER_NONE)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_get_policy_for_iqn(export, &g_iqn)
              == IQN_FILTER_NONE);

    export_delete(export);
}

ut_test(export_iscsi_get_policy_for_null_export_correct_return_IQN_FILTER_NONE)
{
    UT_ASSERT(export_iscsi_get_policy_for_iqn(NULL, &g_iqn)
              == IQN_FILTER_NONE);
}

ut_test(export_iscsi_get_policy_for_invalid_iqn_correct_return_IQN_FILTER_NONE)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_ACCEPT));
    UT_ASSERT_EQUAL(IQN_FILTER_NONE, export_iscsi_get_policy_for_iqn(export, NULL));

    export_delete(export);
}

ut_test(export_iscsi_get_policy_for_iqn_with_wildcard_return_correct_policy)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    iqn_t iqn_pattern;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    iqn_from_str(&iqn_pattern, "iqn.2010-06.*");
    UT_ASSERT_EQUAL(EXA_SUCCESS, export_iscsi_add_iqn_filter(export, &iqn_pattern,
                                                             IQN_FILTER_ACCEPT));
    UT_ASSERT_EQUAL(IQN_FILTER_ACCEPT, export_iscsi_get_policy_for_iqn(export, &g_iqn));
    UT_ASSERT_EQUAL(EXA_SUCCESS, export_iscsi_remove_iqn_filter(export, &iqn_pattern));

    iqn_from_str(&iqn_pattern, "*ut_test_iqn");
    UT_ASSERT_EQUAL(EXA_SUCCESS, export_iscsi_add_iqn_filter(export, &iqn_pattern,
                                                             IQN_FILTER_ACCEPT));
    UT_ASSERT_EQUAL(IQN_FILTER_ACCEPT, export_iscsi_get_policy_for_iqn(export, &g_iqn));
    UT_ASSERT_EQUAL(EXA_SUCCESS, export_iscsi_remove_iqn_filter(export, &iqn_pattern));

    iqn_from_str(&iqn_pattern, "iqn.2010-06.com.*:ut_test_iqn");
    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_add_iqn_filter(export, &iqn_pattern, IQN_FILTER_ACCEPT));
    UT_ASSERT_EQUAL(IQN_FILTER_ACCEPT, export_iscsi_get_policy_for_iqn(export, &g_iqn));
    UT_ASSERT_EQUAL(EXA_SUCCESS, export_iscsi_remove_iqn_filter(export, &iqn_pattern));

    export_delete(export);
}


ut_test(export_iscsi_remove_iqn_filter_correct_return_EXA_SUCCESS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_ACCEPT)
              == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_remove_iqn_filter(export, &g_iqn) == EXA_SUCCESS);

    export_delete(export);
}

ut_test(export_iscsi_remove_iqn_filter_null_export_return_LUM_ERR_INVALID_EXPORT)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_remove_iqn_filter(NULL, &g_iqn));
}

ut_test(export_iscsi_remove_iqn_filter_null_iqn_return_LUM_ERR_INVALID_IQN_FILTER)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_IQN_FILTER,
                    export_iscsi_remove_iqn_filter(export, NULL));

    export_delete(export);
}

ut_test(export_iscsi_remove_iqn_filter_for_wrong_export_return_LUM_ERR_INVALID_EXPORT)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_remove_iqn_filter(export, &g_iqn));

    export_delete(export);
}

ut_test(export_iscsi_remove_unexistent_iqn_filter_correct_return_EXA_SUCCESS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_REJECT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_ACCEPT)
              == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_remove_iqn_filter(export, &g_non_existent_iqn)
              == -LUM_ERR_IQN_FILTER_NOT_FOUND);

    export_delete(export);
}

ut_test(export_iscsi_remove_iqn_filter_correct_not_last_return_EXA_SUCCESS)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.0", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_REJECT)
              == EXA_SUCCESS);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.1", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_REJECT)
              == EXA_SUCCESS);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.2", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_REJECT)
              == EXA_SUCCESS);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.0", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_remove_iqn_filter(export, &iqn) == EXA_SUCCESS);

    /* Now verify the correct one has been removed, using the policy */
    os_snprintf(iqn_str, sizeof(iqn_str), "%s.0", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_get_policy_for_iqn(export, &iqn)
              == IQN_FILTER_ACCEPT);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.1", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_get_policy_for_iqn(export, &iqn)
              == IQN_FILTER_REJECT);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.2", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_get_policy_for_iqn(export, &iqn)
              == IQN_FILTER_REJECT);

    export_delete(export);
}

ut_test(export_iscsi_get_iqn_filters_number)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_get_iqn_filters_number(export) == 0);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.0", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_REJECT)
              == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_get_iqn_filters_number(export) == 1);

    UT_ASSERT(export_iscsi_get_iqn_filters_number(NULL) == EXPORT_INVALID_PARAM);

    export_delete(export);
}

ut_test(export_iscsi_get_iqn_filters_number_null_export)
{
    UT_ASSERT(export_iscsi_get_iqn_filters_number(NULL) == EXPORT_INVALID_PARAM);
}

ut_test(export_iscsi_get_iqn_filters_number_wrong_export)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_get_iqn_filters_number(export)
              == EXPORT_INVALID_PARAM);

    export_delete(export);
}

ut_test(export_iscsi_get_nth_iqn_filter)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn[3];
    int i;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    for (i = 0; i < 3; i++)
    {
        os_snprintf(iqn_str, sizeof(iqn_str), "%s.%d", IQN_STR, i);
        iqn_from_str(&iqn[i], iqn_str);
        UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn[i], IQN_FILTER_REJECT)
                  == EXA_SUCCESS);
    }

    UT_ASSERT(export_iscsi_get_iqn_filters_number(export) == 3);

    for (i = 0; i < 3; i++)
    {
        const iqn_filter_t *filter = export_iscsi_get_nth_iqn_filter(export, i);
        UT_ASSERT(iqn_is_equal(&iqn[i], iqn_filter_get_pattern(filter)));
        UT_ASSERT_EQUAL(IQN_FILTER_REJECT, iqn_filter_get_policy(filter));
    }

    export_delete(export);
}

ut_test(export_iscsi_get_nth_iqn_filter_wrong_export)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_get_nth_iqn_filter(export, 0) == NULL);

    export_delete(export);
}

ut_test(export_iscsi_get_nth_iqn_filter_wrong_number)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    const iqn_filter_t *filter;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    UT_ASSERT(export_iscsi_add_iqn_filter(export, &g_iqn, IQN_FILTER_REJECT)
              == EXA_SUCCESS);

    filter = export_iscsi_get_nth_iqn_filter(export, 0);
    UT_ASSERT(filter != NULL);
    UT_ASSERT(iqn_is_equal(iqn_filter_get_pattern(filter), &g_iqn));

    UT_ASSERT(export_iscsi_get_nth_iqn_filter(export, -1) == NULL);
    UT_ASSERT(export_iscsi_get_nth_iqn_filter(export, 1) == NULL);

    export_delete(export);
}

ut_test(export_iscsi_get_nth_iqn_filter_NULL_export)
{
    UT_ASSERT(export_iscsi_get_nth_iqn_filter(NULL, 0) == NULL);
}

ut_test(export_iscsi_get_nth_iqn_filter_policy)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn[3];
    int i;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    for (i = 0; i < 3; i++)
    {
        os_snprintf(iqn_str, sizeof(iqn_str), "%s.%d", IQN_STR, i);
        iqn_from_str(&iqn[i], iqn_str);
        UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn[i], IQN_FILTER_REJECT)
                  == EXA_SUCCESS);
    }

    UT_ASSERT(export_iscsi_get_iqn_filters_number(export) == 3);

    for (i = 0; i < 3; i++)
    {
        const iqn_filter_t *filter = export_iscsi_get_nth_iqn_filter(export, i);
        UT_ASSERT_EQUAL(IQN_FILTER_REJECT, iqn_filter_get_policy(filter));
    }

    export_delete(export);
}

ut_test(export_iscsi_clear_iqn_filters)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn[3];
    int i;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    for (i = 0; i < 3; i++)
    {
        os_snprintf(iqn_str, sizeof(iqn_str), "%s.%d", IQN_STR, i);
        iqn_from_str(&iqn[i], iqn_str);
        UT_ASSERT(export_iscsi_add_iqn_filter(export, &iqn[i], IQN_FILTER_REJECT)
                  == EXA_SUCCESS);
    }

    UT_ASSERT(export_iscsi_clear_iqn_filters(export) == EXA_SUCCESS);
    UT_ASSERT(export_iscsi_get_iqn_filters_number(export) == 0);

    export_delete(export);
}

ut_test(export_iscsi_clear_iqn_filters_policy)
{
    exa_uuid_t uuid;
    export_t *export = NULL;
    char iqn_str[IQN_MAX_LEN + 1];
    iqn_t iqn;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_iscsi(&uuid, LUN, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.0", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_REJECT));

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.1", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_ACCEPT));

    os_snprintf(iqn_str, sizeof(iqn_str), "%s.2", IQN_STR);
    iqn_from_str(&iqn, iqn_str);
    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_add_iqn_filter(export, &iqn, IQN_FILTER_REJECT));

    UT_ASSERT_EQUAL(3, export_iscsi_get_iqn_filters_number(export));
    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_clear_iqn_filters_policy(export, IQN_FILTER_REJECT));
    UT_ASSERT_EQUAL(1, export_iscsi_get_iqn_filters_number(export));
    UT_ASSERT_EQUAL(EXA_SUCCESS,
                    export_iscsi_clear_iqn_filters_policy(export, IQN_FILTER_ACCEPT));
    UT_ASSERT_EQUAL(0, export_iscsi_get_iqn_filters_number(export));

    export_delete(export);
}

ut_test(export_iscsi_clear_iqn_filters_wrong_export)
{
    exa_uuid_t uuid;
    export_t *export = NULL;

    uuid_scan(EXPORT_UUID, &uuid);
    export = export_new_bdev(&uuid, BDEV_PATH);
    UT_ASSERT(export != NULL);

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_clear_iqn_filters(export));

    export_delete(export);
}

ut_test(export_iscsi_clear_iqn_filters_NULL_export)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_EXPORT, export_iscsi_clear_iqn_filters(NULL));
}
