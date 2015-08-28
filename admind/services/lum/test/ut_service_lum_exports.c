/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "admind/include/service_lum.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_group.h"
#include "common/include/exa_error.h"
#include "common/include/exa_mkstr.h"
#include "os/include/strlcpy.h"
#include "os/include/os_random.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"

static struct adm_group group_1;
static struct adm_group group_2;

static void init_groups()
{
    strlcpy(group_1.name, "group_1", sizeof(group_1.name));
    uuid_generate(&(group_1.uuid));
    UT_ASSERT_EQUAL(EXA_SUCCESS, adm_group_insert_group(&group_1));

    strlcpy(group_2.name, "group_2", sizeof(group_2.name));
    uuid_generate(&(group_2.uuid));
    UT_ASSERT_EQUAL(EXA_SUCCESS, adm_group_insert_group(&group_2));
}

ut_setup()
{
    /* This static stuff is useless except for running the unit test without
       forking each test case */
    static bool random_initialized = false;
    if (!random_initialized)
    {
        os_random_init();
        init_groups();
    }
    random_initialized = true;
}

ut_cleanup()
{
}

static void insert_volume(lun_t _lun, struct adm_group *group,
                          struct adm_volume *volume, int32_t gen_name)
{
    adm_export_t *adm_export;
    export_t *export;

    os_snprintf(volume->name, sizeof(volume->name), "volume_%d", gen_name);
    uuid_generate(&volume->uuid);

    export = export_new_iscsi(&volume->uuid, _lun, IQN_FILTER_ACCEPT);
    UT_ASSERT(export != NULL);

    adm_export = adm_export_alloc();
    UT_ASSERT(adm_export != NULL);
    adm_export_set(adm_export, export, false);

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_insert_export(adm_export));
    UT_ASSERT_EQUAL(EXA_SUCCESS, adm_group_insert_volume(group, volume));
}

ut_test(get_new_lun)
{
    static struct adm_volume volumes[256];

    lun_t _lun;
    lun_t i;

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_get_new_lun(&_lun));
    UT_ASSERT_EQUAL(0, _lun);

    for (i = 0; i < 100; i++)
        insert_volume(i, &group_1, &volumes[i], i);

    for (i = 100; i < 234; i++)
        insert_volume(i, &group_2, &volumes[i], i);

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_get_new_lun(&_lun));
    UT_ASSERT_EQUAL(234, _lun);

    for (i = 234; i < MAX_LUNS; i++)
        insert_volume(i, &group_2, &volumes[i], i);

    UT_ASSERT(EXA_SUCCESS != lum_get_new_lun(&_lun));
}


ut_test(lun_is_available)
{
    static struct adm_volume volume_1;
    static struct adm_volume volume_2;

    insert_volume(123, &group_1, &volume_1, 1);

    insert_volume(234, &group_2, &volume_2, 2);

    UT_ASSERT(  lum_lun_is_available(0));
    UT_ASSERT(! lum_lun_is_available(123));
    UT_ASSERT(! lum_lun_is_available(234));
    UT_ASSERT(  lum_lun_is_available(MAX_LUNS - 1));
}

#define BDEV_EXPORT_UUID_1  "56819C58:6FB043FB:85DA5E22:0915AF8B"
#define BDEV_EXPORT_UUID_2  "56819C58:6FB043FB:85DA5E22:0915AF8C"

#define BDEV_EXPORT_PATH_1  "/dev/exa/group/vol1"
#define BDEV_EXPORT_PATH_2  "/dev/exa/group/vol2"

#define EXPORT_XML_BDEV_CORRECT     \
    "<?xml version=\"1.0\" ?>\n"                      \
    "<exportlist format_version=\""                   \
    ADM_EXPORTS_FILE_VERSION_STR "\" version=\"2\">\n"\
    "    <export uuid=\"" BDEV_EXPORT_UUID_1 "\"\n"   \
    "            type=\"bdev\"\n"                     \
    "            path=\"" BDEV_EXPORT_PATH_1 "\"/>\n" \
    "    <export uuid=\"" BDEV_EXPORT_UUID_2 "\"\n"   \
    "            type=\"bdev\"\n"                     \
    "            path=\"" BDEV_EXPORT_PATH_2 "\"/>\n" \
    "</exportlist>"

ut_test(lum_exports_parse_xml_bdev_correct)
{
    const adm_export_t *adm_export1, *adm_export2;
    exa_uuid_t uuid1, uuid2;

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(EXPORT_XML_BDEV_CORRECT));
    UT_ASSERT_EQUAL(2, lum_exports_get_version());

    UT_ASSERT_EQUAL(2, lum_exports_get_number());

    uuid_scan(BDEV_EXPORT_UUID_1, &uuid1);
    adm_export1 = lum_exports_get_nth_export(0);
    UT_ASSERT(adm_export1 != NULL);

    UT_ASSERT(uuid_is_equal(adm_export_get_uuid(adm_export1), &uuid1));
    UT_ASSERT_EQUAL(EXPORT_BDEV, adm_export_get_type(adm_export1));
    UT_ASSERT_EQUAL_STR(BDEV_EXPORT_PATH_1, adm_export_get_path(adm_export1));

    uuid_scan(BDEV_EXPORT_UUID_2, &uuid2);
    adm_export2 = lum_exports_get_nth_export(1);
    UT_ASSERT(adm_export2 != NULL);

    UT_ASSERT(uuid_is_equal(adm_export_get_uuid(adm_export2), &uuid2));
    UT_ASSERT_EQUAL(EXPORT_BDEV, adm_export_get_type(adm_export2));
    UT_ASSERT_EQUAL_STR(BDEV_EXPORT_PATH_2, adm_export_get_path(adm_export2));

    lum_exports_clear();
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
}

#define EXPORT_XML_BDEV_FORMAT_VERSION_WRONG            \
    "<?xml version=\"1.0\" ?>\n"                        \
    "<exportlist format_version=\"1234567890\" version=\"2\">\n"\
    "    <export uuid=\"" BDEV_EXPORT_UUID_1 "\"\n"     \
    "            type=\"bdev\"\n"                       \
    "            path=\"" BDEV_EXPORT_PATH_1 "\"/>\n"   \
    "    <export uuid=\"" BDEV_EXPORT_UUID_2 "\"\n"     \
    "            type=\"bdev\"\n"                       \
    "            path=\"" BDEV_EXPORT_PATH_2 "\"/>\n"   \
    "</exportlist>"

ut_test(lum_exports_parse_xml_bdev_incorrect_format_version)
{
    UT_ASSERT_EQUAL(-EXA_ERR_XML_PARSE,
	lum_exports_parse_from_xml(EXPORT_XML_BDEV_FORMAT_VERSION_WRONG));
}

#define ISCSI_EXPORT_UUID_1  "56819C58:6FB043FB:85DA5E22:0915AF8B"
#define ISCSI_EXPORT_UUID_2  "56819C58:6FB043FB:85DA5E22:0915AF8C"

#define EXPORT_XML_ISCSI_CORRECT                                        \
    "<?xml version=\"1.0\" ?>\n"                                        \
    "<exportlist format_version=\""                                     \
    ADM_EXPORTS_FILE_VERSION_STR "\" version=\"2\">\n"                  \
    "    <export uuid=\"" ISCSI_EXPORT_UUID_1 "\"\n"                    \
    "            type=\"iscsi\"\n"                                      \
    "            lun=\"1\"\n"                                           \
    "            filter_policy=\"accept\">\n"                           \
    "        <filter iqn=\"iqn.seanodes.com\" policy=\"reject\"/>\n"    \
    "        <filter iqn=\"iqn.*.com\" policy=\"accept\"/>\n"           \
    "    </export>\n"                                                   \
    "    <export uuid=\"" ISCSI_EXPORT_UUID_2 "\"\n"                    \
    "            type=\"iscsi\"\n"                                      \
    "            lun=\"5\"\n"                                           \
    "            filter_policy=\"reject\">\n"                           \
    "        <filter iqn=\"iqn.*.org\" policy=\"accept\"/>\n"           \
    "        <filter iqn=\"iqn.microsoft.com\" policy=\"reject\"/>\n"   \
    "    </export>\n"                                                   \
    "</exportlist>"

#define EXPORT_XML_BDEV_AND_ISCSI_CORRECT                               \
    "<?xml version=\"1.0\" ?>\n"                                        \
    "<exportlist format_version=\""                                     \
    ADM_EXPORTS_FILE_VERSION_STR "\" version=\"2\">\n"                  \
    "    <export uuid=\"" BDEV_EXPORT_UUID_1 "\"\n"                     \
    "            type=\"bdev\"\n"                                       \
    "            path=\"" BDEV_EXPORT_PATH_1 "\"/>\n"                   \
    "    <export uuid=\"" BDEV_EXPORT_UUID_2 "\"\n"                     \
    "            type=\"bdev\"\n"                                       \
    "            path=\"" BDEV_EXPORT_PATH_2 "\"/>\n"                   \
    "    <export uuid=\"" ISCSI_EXPORT_UUID_1 "\"\n"                    \
    "            type=\"iscsi\"\n"                                      \
    "            lun=\"1\"\n"                                           \
    "            filter_policy=\"accept\">\n"                           \
    "        <filter iqn=\"iqn.seanodes.com\" policy=\"reject\"/>\n"    \
    "        <filter iqn=\"iqn.*.com\" policy=\"accept\"/>\n"           \
    "    </export>\n"                                                   \
    "    <export uuid=\"" ISCSI_EXPORT_UUID_2 "\"\n"                    \
    "            type=\"iscsi\"\n"                                      \
    "            lun=\"5\"\n"                                           \
    "            filter_policy=\"reject\">\n"                           \
    "        <filter iqn=\"iqn.*.org\" policy=\"accept\"/>\n"           \
    "        <filter iqn=\"iqn.microsoft.com\" policy=\"reject\"/>\n"   \
    "    </export>\n"                                                   \
    "</exportlist>"

ut_test(lum_exports_parse_xml_iscsi_correct)
{
    const adm_export_t *adm_export1, *adm_export2, *adm_export3;
    const export_t *export1, *export2;
    exa_uuid_t uuid1, uuid2;
    const iqn_filter_t *filter;

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(EXPORT_XML_ISCSI_CORRECT));
    UT_ASSERT_EQUAL(2, lum_exports_get_version());

    UT_ASSERT_EQUAL(2, lum_exports_get_number());

    /* First export */
    uuid_scan(ISCSI_EXPORT_UUID_1, &uuid1);
    adm_export1 = lum_exports_get_nth_export(0);
    UT_ASSERT(adm_export1 != NULL);

    export1 = adm_export1->desc;

    UT_ASSERT(uuid_is_equal(export_get_uuid(export1), &uuid1));
    UT_ASSERT_EQUAL(EXPORT_ISCSI, export_get_type(export1));

    UT_ASSERT_EQUAL(1, export_iscsi_get_lun(export1));

    UT_ASSERT_EQUAL(IQN_FILTER_ACCEPT, export_iscsi_get_filter_policy(export1));
    UT_ASSERT_EQUAL(2, export_iscsi_get_iqn_filters_number(export1));

    filter = export_iscsi_get_nth_iqn_filter(export1, 0);
    UT_ASSERT(filter != NULL);
    UT_ASSERT_EQUAL_STR("iqn.seanodes.com", iqn_to_str(iqn_filter_get_pattern(filter)));
    UT_ASSERT_EQUAL(IQN_FILTER_REJECT, iqn_filter_get_policy(filter));

    filter = export_iscsi_get_nth_iqn_filter(export1, 1);
    UT_ASSERT(filter != NULL);
    UT_ASSERT_EQUAL_STR("iqn.*.com", iqn_to_str(iqn_filter_get_pattern(filter)));
    UT_ASSERT_EQUAL(IQN_FILTER_ACCEPT, iqn_filter_get_policy(filter));

    /* Second export */
    uuid_scan(ISCSI_EXPORT_UUID_2, &uuid2);
    adm_export2 = lum_exports_get_nth_export(1);
    UT_ASSERT(adm_export2 != NULL);

    export2 = adm_export2->desc;

    UT_ASSERT(uuid_is_equal(export_get_uuid(export2), &uuid2));
    UT_ASSERT_EQUAL(EXPORT_ISCSI, export_get_type(export2));

    UT_ASSERT_EQUAL(5, export_iscsi_get_lun(export2));

    UT_ASSERT_EQUAL(IQN_FILTER_REJECT, export_iscsi_get_filter_policy(export2));
    UT_ASSERT_EQUAL(2, export_iscsi_get_iqn_filters_number(export2));

    filter = export_iscsi_get_nth_iqn_filter(export2, 0);
    UT_ASSERT(filter != NULL);
    UT_ASSERT_EQUAL_STR("iqn.*.org", iqn_to_str(iqn_filter_get_pattern(filter)));
    UT_ASSERT_EQUAL(IQN_FILTER_ACCEPT, iqn_filter_get_policy(filter));

    filter = export_iscsi_get_nth_iqn_filter(export2, 1);
    UT_ASSERT_EQUAL_STR("iqn.microsoft.com", iqn_to_str(iqn_filter_get_pattern(filter)));
    UT_ASSERT_EQUAL(IQN_FILTER_REJECT, iqn_filter_get_policy(filter));

    /* Non-existing */
    adm_export3 = lum_exports_get_nth_export(2);
    UT_ASSERT(adm_export3 == NULL);

    lum_exports_clear();
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
}

ut_test(lum_exports_dump_to_xml_correct)
{
    exa_error_code err;
    char *contents;

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(EXPORT_XML_BDEV_CORRECT));
    contents = lum_exports_to_xml(&err);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL_STR(EXPORT_XML_BDEV_CORRECT, contents);

    os_free(contents);

    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(EXPORT_XML_ISCSI_CORRECT));
    contents = lum_exports_to_xml(&err);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL_STR(EXPORT_XML_BDEV_AND_ISCSI_CORRECT, contents);

    os_free(contents);
}

#define EXPORT_XML_TMPL_BDEV "<?xml version=\"1.0\" ?>\
<exportlist format_version=\"" ADM_EXPORTS_FILE_VERSION_STR \
        "\" version=\"2\">\
	<export uuid=\"%s\"\
	    type=\"bdev\"\
	    path=\"/dev/exa/group/volume\"/>\
</exportlist>"

#define EXPORT_XML_TMPL_ISCSI "<?xml version=\"1.0\" ?>\
<exportlist format_version=\"" ADM_EXPORTS_FILE_VERSION_STR \
        "\" version=\"2\">\
	<export uuid=\"%s\"\
	    type=\"iscsi\"\
	    lun=\"%s\"\
	    filter_policy=\"%s\">\
                <%s iqn=\"%s\" policy=\"%s\"/>\
        </export>\
</exportlist>"

#define EXPORT_XML_TMPL_ISCSI_NO_FILTER "<?xml version=\"1.0\" ?>\
<exportlist format_version=\"" ADM_EXPORTS_FILE_VERSION_STR \
        "\" version=\"2\">\
	<export uuid=\"%s\"\
	    type=\"iscsi\"\
	    lun=\"%s\"\
	    filter_policy=\"%s\">\
        </export>\
</exportlist>"

static const char *get_bdev_xml(const char *uuid)
{
     static char xml[8192];

     UT_ASSERT(os_snprintf(xml, sizeof(xml), EXPORT_XML_TMPL_BDEV, uuid) < sizeof(xml));

     return xml;
}

static const char *get_iscsi_xml(const char *uuid, const char *lun,
                           const char *filter_policy, const char *filter_param,
                           const char *iqn, const char *iqn_policy)
{
     static char xml[8192];

     UT_ASSERT(os_snprintf(xml, sizeof(xml), EXPORT_XML_TMPL_ISCSI, uuid, lun,
                           filter_policy, filter_param,iqn, iqn_policy)
                            < sizeof(xml));

     return xml;
}

static const char *get_iscsi_xml_no_filter(const char *uuid, const char *lun,
                           const char *filter_policy)
{
     static char xml[8192];

     UT_ASSERT(os_snprintf(xml, sizeof(xml), EXPORT_XML_TMPL_ISCSI_NO_FILTER,
                           uuid, lun, filter_policy)
                            < sizeof(xml));

     return xml;
}

ut_test(lum_exports_parse_empty_exportlist)
{
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        "<exportlist format_version=\""
                        ADM_EXPORTS_FILE_VERSION_STR "\" version=\"2\">"
                        "</exportlist>"));
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        "<exportlist format_version=\"" ADM_EXPORTS_FILE_VERSION_STR
                        "\" version=\"2\"/>"));
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        "<exportlist format_version=\"" ADM_EXPORTS_FILE_VERSION_STR
                        "\" version=\"2\">\n</exportlist>"));
}

ut_test(lum_exports_parse_no_format_version_considers_version_1)
{
    /* This unit test will fatally fail once we increment the exports
     * file format version. We'll have to remove it then.
     */
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        "<exportlist version=\"2\">"
                        "</exportlist>"));
}

ut_test(lum_exports_parse_wrong_xml)
{
    UT_ASSERT_EQUAL(-EXA_ERR_XML_PARSE, lum_exports_parse_from_xml(
                        "HAHA"));
    UT_ASSERT_EQUAL(-EXA_ERR_XML_PARSE, lum_exports_parse_from_xml(
                        "<exportlist />"));
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
}

ut_test(lum_exports_parse_wrong_type)
{
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM,
                    lum_exports_parse_from_xml(
                    "<exportlist format_version=\"" ADM_EXPORTS_FILE_VERSION_STR
                    "\" version=\"2\"><export uuid="
                    "\"56819C58:6FB043FB:85DA5E22:0915AF8B\" type=\"blah\"/>"
                    "</exportlist>"));
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
}

ut_test(lum_exports_parse_wrong_bdev_params_from_xml)
{
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM, lum_exports_parse_from_xml(
                        get_bdev_xml("wrong_uuid")));
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        get_bdev_xml("56819C58:6FB043FB:85DA5E22:0915AF8B")));
    UT_ASSERT_EQUAL(1, lum_exports_get_number());
    lum_exports_clear();
}

ut_test(lum_exports_parse_wrong_iscsi_params_from_xml)
{
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM,
                    lum_exports_parse_from_xml(
                        get_iscsi_xml("wrong_uuid", "8", "accept", "filter", "iqn.*", "accept")));
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        get_iscsi_xml_no_filter("56819C58:6FB043FB:85DA5E22:0915AF8B", "8", "accept")));
    UT_ASSERT_EQUAL(EXA_SUCCESS, lum_exports_parse_from_xml(
                        get_iscsi_xml("56819C58:6FB043FB:85DA5E22:0915AF8B", "8", "accept", "filter", "iqn.*", "accept")));
    UT_ASSERT_EQUAL(2, lum_exports_get_number());
    lum_exports_clear();
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM, lum_exports_parse_from_xml(
                        get_iscsi_xml("56819C58:6FB043FB:85DA5E22:0915AF8B", "s", "accept", "filter", "iqn.*", "accept")));
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM, lum_exports_parse_from_xml(
                        get_iscsi_xml("56819C58:6FB043FB:85DA5E22:0915AF8B", "8", "reject", "filter", "iqn.*", "xx")));
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM, lum_exports_parse_from_xml(
                        get_iscsi_xml("56819C58:6FB043FB:85DA5E22:0915AF8B", "8", "x", "filter", "iqn.*", "accept")));
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_PARAM, lum_exports_parse_from_xml(
                        get_iscsi_xml("56819C58:6FB043FB:85DA5E22:0915AF8B", "8", "accept", "filter", "iqn.*", "s")));
    UT_ASSERT_EQUAL(0, lum_exports_get_number());
}
