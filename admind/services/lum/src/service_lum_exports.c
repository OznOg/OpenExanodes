/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <sys/stat.h>
#ifndef WIN32
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

#include "admind/services/lum/include/service_lum_exports.h"
#include "admind/services/lum/include/adm_export.h"
#include "admind/include/service_lum.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_file_ops.h"
#include "admind/src/rpc.h"
#include "lum/export/include/export.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_env.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_error.h"
#include "log/include/log.h"

static adm_export_t *adm_exports[EXA_MAX_NUM_EXPORTS];
static int num_exports = 0;
static uint64_t exports_version = EXPORTS_VERSION_DEFAULT;

/* FIXME Get rid of this forward declaration */
static adm_export_t *lum_exports_get_export_by_uuid(const exa_uuid_t *uuid);

bool lum_lun_is_available(lun_t lun)
{
    const adm_export_t *adm_export;
    int i;

    EXA_ASSERT(LUN_IS_VALID(lun));

    lum_exports_for_each_export(i, adm_export)
    {
        if (adm_export_get_type(adm_export) == EXPORT_ISCSI
            && adm_export_get_lun(adm_export) == lun)
            return false;
    }

    return true;
}

int lum_get_new_lun(lun_t *const _lun)
{
    bool lun_free[MAX_LUNS];
    lun_t lun;
    const adm_export_t *adm_export;
    int i;

    for (lun = 0; lun < MAX_LUNS; lun++)
        lun_free[lun] = true;

    /* Fill array with used luns */
    lum_exports_for_each_export(i, adm_export)
    {
        lun_t l;

        if (adm_export_get_type(adm_export) != EXPORT_ISCSI)
            continue;

        l = adm_export_get_lun(adm_export);
        if (l != LUN_NONE)
            lun_free[l] = false;
    }

    for (lun = 0; lun < MAX_LUNS; lun++)
        if (lun_free[lun])
        {
            *_lun = lun;
            return EXA_SUCCESS;
        }

    return -LUN_ERR_NO_LUN_AVAILABLE;
}

/******************************************************************************
 *                         FIXME: TEMPORARY GLUE CODE                         *
 ******************************************************************************/

void lum_exports_set_readonly_by_uuid(const exa_uuid_t *uuid, bool readonly)
{
    const adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);

    export_set_readonly(adm_export->desc, readonly);
}

lun_t lum_exports_iscsi_get_lun_by_uuid(const exa_uuid_t *uuid)
{
     const adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     return adm_export_get_lun(adm_export);
}

int lum_exports_iscsi_set_lun_by_uuid(const exa_uuid_t *uuid, lun_t lun)
{
     adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     int err;

     if (adm_export == NULL || adm_export_get_type(adm_export) != EXPORT_ISCSI)
         return -EXA_ERR_EXPORT_NOT_FOUND;

     if (!LUN_IS_VALID(lun))
         return -LUN_ERR_INVALID_VALUE;

     if (!lum_lun_is_available(lun))
         return -LUN_ERR_LUN_BUSY;

     err = adm_export_set_lun(adm_export, lun);

     if (err != EXA_SUCCESS)
         return err;

     lum_exports_increment_version();

     return lum_serialize_exports();
}

iqn_filter_policy_t lum_exports_iscsi_get_filter_policy_by_uuid(const exa_uuid_t *uuid)
{
     const adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     return export_iscsi_get_filter_policy(adm_export->desc);
}

int lum_exports_iscsi_set_filter_policy_by_uuid(const exa_uuid_t *uuid,
                                       iqn_filter_policy_t policy)
{
     adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     int err;

     err = export_iscsi_set_filter_policy(adm_export->desc, policy);

     if (err != EXA_SUCCESS)
         return err;

     lum_exports_increment_version();

     return lum_serialize_exports();
}

exa_error_code lum_exports_iscsi_add_iqn_filter_by_uuid(const exa_uuid_t *uuid,
                                           const char *iqn_pattern_str,
                                           iqn_filter_policy_t policy)
{
    adm_export_t *adm_export;
    iqn_t iqn_pattern;
    exa_error_code err;

    if (iqn_from_str(&iqn_pattern, iqn_pattern_str) != 0)
        return -EXA_ERR_INVALID_VALUE;

    adm_export = lum_exports_get_export_by_uuid(uuid);

    err = export_iscsi_add_iqn_filter(adm_export->desc, &iqn_pattern, policy);

    if (err != EXA_SUCCESS)
        return err;

    lum_exports_increment_version();

    return lum_serialize_exports();
}

exa_error_code lum_exports_iscsi_remove_iqn_filter_by_uuid(const exa_uuid_t *uuid,
                                                           const char *iqn_pattern_str)
{
    adm_export_t *adm_export;
    iqn_t iqn_pattern;
    exa_error_code err;

    if (iqn_from_str(&iqn_pattern, iqn_pattern_str) != 0)
        return -EXA_ERR_INVALID_VALUE;

    adm_export = lum_exports_get_export_by_uuid(uuid);

    err = export_iscsi_remove_iqn_filter(adm_export->desc, &iqn_pattern);

    if (err != EXA_SUCCESS)
        return err;

    lum_exports_increment_version();

    return lum_serialize_exports();
}

exa_error_code lum_exports_iscsi_clear_iqn_filters_policy_by_uuid(const exa_uuid_t *uuid,
                                                            iqn_filter_policy_t policy)
{
     adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     exa_error_code err;

     err = export_iscsi_clear_iqn_filters_policy(adm_export->desc, policy);

     if (err != EXA_SUCCESS)
         return err;

     lum_exports_increment_version();

     return lum_serialize_exports();
}

int lum_exports_iscsi_get_iqn_filters_number_by_uuid(const exa_uuid_t *uuid)
{
     adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     return export_iscsi_get_iqn_filters_number(adm_export->desc);
}

const iqn_t *lum_exports_iscsi_get_nth_iqn_filter_by_uuid(const exa_uuid_t *uuid, int n)
{
     const adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     const iqn_filter_t *filter = export_iscsi_get_nth_iqn_filter(adm_export->desc, n);
     return iqn_filter_get_pattern(filter);
}

iqn_filter_policy_t lum_exports_iscsi_get_nth_iqn_filter_policy_by_uuid(
                                            const exa_uuid_t *uuid, int n)
{
     const adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);
     const iqn_filter_t *filter = export_iscsi_get_nth_iqn_filter(adm_export->desc, n);
     return iqn_filter_get_policy(filter);
}

export_type_t lum_exports_get_type_by_uuid(const exa_uuid_t *uuid)
{
    const adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);

    return adm_export_get_type(adm_export);
}

/******************************************************************************
 *                         FIXME: END OF TEMPORARY GLUE CODE                  *
 ******************************************************************************/

static export_t *lum_exports_parse_bdev_export(const exa_uuid_t *uuid,
                                                   const xmlNode *node,
                                                   const xmlDocPtr doc)
{
    export_t *export = NULL;
    const char *path;

    path = xml_get_prop(node, "path");
    export = export_new_bdev(uuid, path);
    return export;
}

static exa_error_code lum_exports_parse_iqn_filter(export_t *export, const xmlNode * node)
{
    iqn_t iqn;
    const char *s_filter_policy;
    iqn_filter_policy_t filter_policy = EXPORT_INVALID_VALUE;
    exa_error_code result;

    if (xmlStrcmp(node->name, BAD_CAST("filter")) != 0)
        return EXA_ERR_INVALID_VALUE;

    iqn_from_str(&iqn, xml_get_prop(node, "iqn"));

    s_filter_policy = xml_get_prop(node, "policy");
    filter_policy = iqn_filter_policy_from_str(s_filter_policy);

    result = export_iscsi_add_iqn_filter(export, &iqn, filter_policy);
    if (result != EXA_SUCCESS)
        exalog_error("Failed adding filter_policy "IQN_FMT":%s: %d",
                     IQN_VAL(&iqn), s_filter_policy, result);
    return result;
}

static export_t *lum_exports_parse_iscsi_export(const exa_uuid_t *uuid,
                                                    const xmlNode * node,
                                                    const xmlDocPtr doc)
{
    export_t *export = NULL;
    const char *s_lun, *s_filter_policy;
    lun_t lun;
    iqn_filter_policy_t filter_policy = EXPORT_INVALID_VALUE;
    const xmlNode * filter;
    xmlNodeSetPtr filter_set;
    int i;

    s_lun = xml_get_prop(node, "lun");
    lun = lun_from_str(s_lun);
    if (!LUN_IS_VALID(lun))
    {
        exalog_error("Invalid lun %s, skipping export " UUID_FMT,
                     s_lun, UUID_VAL(uuid));
        return NULL;
    }

    s_filter_policy = xml_get_prop(node, "filter_policy");
    filter_policy = iqn_filter_policy_from_str(s_filter_policy);

    export = export_new_iscsi(uuid, lun, filter_policy);
    if (export == NULL)
        return NULL;

    filter_set = xml_conf_xpath_query(doc,
                    "/exportlist/export[@uuid='"UUID_FMT"']/filter",
                    UUID_VAL(uuid));
    xml_conf_xpath_result_for_each(filter_set, filter, i)
    {
        if (lum_exports_parse_iqn_filter(export, filter) != EXA_SUCCESS)
        {
            export_delete(export);
            exalog_error("Failed parsing IQN filter, skipping export " UUID_FMT,
                     UUID_VAL(uuid));
            xml_conf_xpath_free(filter_set);
            return NULL;
        }
    }
    xml_conf_xpath_free(filter_set);

    return export;
}

static exa_error_code lum_exports_parse_export(const xmlNode * node,
                                               const xmlDocPtr doc)
{
    adm_export_t *adm_export;
    export_t *export;
    const char *s_uuid;
    const char *type;
    exa_uuid_t uuid;

    if (xmlStrcmp(node->name, BAD_CAST("export")) != 0)
        return EXA_ERR_INVALID_VALUE;

    s_uuid = xml_get_prop(node, "uuid");
    if (uuid_scan(s_uuid, &uuid) != EXA_SUCCESS)
    {
        exalog_error("Invalid UUID, skipping export %s", s_uuid);
        return -EXA_ERR_INVALID_PARAM;
    }

    type = xml_get_prop(node, "type");

    if (type && !strcmp(type, "iscsi"))
        export = lum_exports_parse_iscsi_export(&uuid, node, doc);
    else if (type && !strcmp(type, "bdev"))
        export = lum_exports_parse_bdev_export(&uuid, node, doc);
    else
    {
        exalog_error("Invalid type %s, skipping export " UUID_FMT,
                     type ? type : "NULL",
                     UUID_VAL(&uuid));
        export = NULL;
    }

    if (export == NULL)
        return -EXA_ERR_INVALID_PARAM;

    adm_export = adm_export_alloc();
    if (adm_export == NULL)
    {
        export_delete(export);
        return -ENOMEM;
    }
    adm_export_set(adm_export, export, false);

    return lum_exports_insert_export(adm_export);
}

int lum_exports_insert_export(adm_export_t *export)
{
    if (num_exports < EXA_MAX_NUM_EXPORTS)
    {
        adm_exports[num_exports] = export;
        num_exports++;
        return EXA_SUCCESS;
    }

    return -ENOSPC;
}

void lum_exports_remove_export_from_uuid(const exa_uuid_t *uuid)
{
    /* FIXME This function could use a lock */

    int i;

    EXA_ASSERT(uuid != NULL);

    for (i = 0; i < EXA_MAX_NUM_EXPORTS; i++)
    {
        if (adm_exports[i] != NULL &&
            (uuid_compare(uuid, export_get_uuid(adm_exports[i]->desc)) == 0))
            break;
    }

    /* FIXME See bug #4528. We should not be in the case the assert occurs, but
     * as export creation is not transactional we cannot be sure the case won't
     * occur.
    EXA_ASSERT(i != EXA_MAX_NUM_EXPORTS);
    */
    if (i == EXA_MAX_NUM_EXPORTS)
    {
        exalog_warning("Cannot delete unknown export uuid=" UUID_FMT ".",
                       UUID_VAL(uuid));
        return;
    }

    adm_export_free(adm_exports[i]);

    /* Shifts all elements in array */
    for ( ; i < EXA_MAX_NUM_EXPORTS - 1; i++)
        adm_exports[i] = adm_exports[i + 1];
    adm_exports[EXA_MAX_NUM_EXPORTS - 1] = NULL;

    num_exports--;
}

uint64_t lum_exports_get_version(void)
{
    return exports_version;
}

void lum_exports_increment_version(void)
{
    exports_version++;
}

void lum_exports_set_version(int version)
{
    exports_version = version;
}

void lum_exports_set_number(int number)
{
    num_exports = number;
}

/* This function is NOT reentrant, as for now it's called only within
 * the commands thread.
 * The day this function could be called by 2+ threads, fixes will be
 * necessary.
 */
static const char *lum_exports_get_exports_file_path(void)
{
    static char path[OS_PATH_MAX];
    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADM_EXPORTS_FILE);
    return path;
}

exa_error_code lum_exports_parse_from_xml(const char *contents)
{
    xmlDocPtr doc = xml_conf_init_from_buf(contents, strlen(contents));
    const xmlNode * root, *node;
    exa_error_code r, result = EXA_SUCCESS;
    const char *s_version;
    xmlNodeSetPtr export_set;
    int i;
    uint32_t format_version;

    if (doc == NULL)
        return -EXA_ERR_XML_PARSE;

    root = xmlDocGetRootElement(doc);

    if (root == NULL)
    {
        xmlFreeDoc(doc);
        return -EXA_ERR_XML_PARSE;
    }

    if (xmlStrcmp(root->name, BAD_CAST("exportlist")))
    {
        xmlFreeDoc(doc);
        return -EXA_ERR_XML_PARSE;
    }

    if ((s_version = xml_get_prop(root, "format_version")) == NULL)
    {
        /* We consider a file without the format_version field to be of
         * default version, as the introduction of format_version is the only
         * difference with older files.
         */
         format_version = EXPORTS_VERSION_DEFAULT;
    }
    else if (to_uint32(s_version, &format_version) < 0)
    {
        xmlFreeDoc(doc);
        return -EXA_ERR_XML_PARSE;
    }

    if (format_version != ADM_EXPORTS_FILE_VERSION)
    {
        exalog_error("Failed parsing exports file: expected version "
                     "%"PRIu32", got version %"PRIu32".",
                     ADM_EXPORTS_FILE_VERSION, format_version);
        xmlFreeDoc(doc);
        return -EXA_ERR_XML_PARSE;
    }

    if ((s_version = xml_get_prop(root, "version")) == NULL)
    {
        xmlFreeDoc(doc);
        return -EXA_ERR_XML_PARSE;
    }

    if (to_uint64(s_version, &exports_version) < 0)
    {
        xmlFreeDoc(doc);
        return -EXA_ERR_XML_PARSE;
    }

    export_set = xml_conf_xpath_query(doc, "/exportlist/export");
    xml_conf_xpath_result_for_each(export_set, node, i)
    {
        if (num_exports >= EXA_MAX_NUM_EXPORTS)
            break;
        if ((r = lum_exports_parse_export(node, doc)) != EXA_SUCCESS)
             result = r;
    }
    xml_conf_xpath_free(export_set);

    xmlFreeDoc(doc);
    return result;
}

exa_error_code lum_deserialize_exports(void)
{
    char path[OS_PATH_MAX];
    char *contents = NULL;
    cl_error_desc_t err;
    exa_error_code r = EXA_SUCCESS;

    strlcpy(path, lum_exports_get_exports_file_path(), sizeof(path));
    contents = adm_file_read_to_str(path, &err);

    if (contents == NULL)
    {
        if (err.code != -ENOENT)
        {
            exalog_error("Failed reading %s: %s (%d)", path, err.msg, err.code);
            return -EXA_ERR_READ_FILE;
        }
        else
            return EXA_SUCCESS;
    }

    lum_exports_clear();

    if ((r = lum_exports_parse_from_xml(contents)) != EXA_SUCCESS)
    {
        exalog_warning("Failed parsing %s: only %d exports were "
                       "successfully parsed.", path, num_exports);
    }

    os_free(contents);
    return r;
}

static char *lum_strconcat(char *old, char *append)
{
    int old_len = strlen(old);
    int app_len = strlen(append);
    char *new = os_malloc(old_len + app_len + 1);

    EXA_ASSERT(os_snprintf(new, old_len + app_len + 1, "%s%s", old, append)
        < old_len + app_len + 1);

    os_free(old);
    return new;
}

#define XML_HEADER "<?xml version=\"1.0\" ?>\n"
#define EXPORTLIST_START "<exportlist format_version=\"" \
    ADM_EXPORTS_FILE_VERSION_STR "\" version=\"%s\">\n"

#define EXPORTLIST_END "</exportlist>"

#define EXPORT_BDEV_FMT "    <export uuid=\""UUID_FMT"\"\n"\
"            type=\"bdev\"\n"\
"            path=\"%s\"/>\n"

#define EXPORT_ISCSI_FMT "    <export uuid=\""UUID_FMT"\"\n"\
"            type=\"iscsi\"\n"\
"            lun=\"%s\"\n"\
"            filter_policy=\"%s\">\n"\
"%s"\
"    </export>\n"

#define IQN_FILTER_FMT "        <filter iqn=\"%s\" policy=\"%s\"/>\n"

static char *lum_export_bdev_to_xml(export_t *export)
{
    char *contents;
    int size;

    size = strlen(EXPORT_BDEV_FMT)
                 + UUID_STR_LEN
                 + strlen(export_bdev_get_path(export))
                 + 1;
    contents = os_malloc(size);

    EXA_ASSERT(os_snprintf(contents, size, EXPORT_BDEV_FMT,
                UUID_VAL(export_get_uuid(export)),
                export_bdev_get_path(export))
               < size);

    return contents;
}

static char *lum_export_iscsi_iqn_filters_to_xml(export_t *export)
{
    char *contents = os_strdup("");
    int i = 0;
    int number = export_iscsi_get_iqn_filters_number(export);

    for (i = 0; i < number; i++)
    {
        char *filter_str;
        const iqn_filter_t *filter;
        const char *pattern_str;
        const char *policy_str;
        int len;

        filter = export_iscsi_get_nth_iqn_filter(export, i);

        pattern_str = iqn_to_str(iqn_filter_get_pattern(filter));
        policy_str = iqn_filter_policy_to_str(iqn_filter_get_policy(filter));

        len = strlen(IQN_FILTER_FMT)
              + strlen(pattern_str)
              + strlen(policy_str);

        filter_str = os_malloc(len + 1);

        EXA_ASSERT(os_snprintf(filter_str, len, IQN_FILTER_FMT,
                               pattern_str, policy_str) < len);

        contents = lum_strconcat(contents, filter_str);

        os_free(filter_str);
    }

    return contents;
}

static char *lum_export_iscsi_to_xml(export_t *export)
{
    const char *lun;
    static const char *filter_policy;
    char *contents;
    char *iqn_filters = "";
    int len;

    lun = lun_to_str(export_iscsi_get_lun(export));
    EXA_ASSERT(lun != NULL);

    filter_policy = iqn_filter_policy_to_str(
        export_iscsi_get_filter_policy(export));

    iqn_filters = lum_export_iscsi_iqn_filters_to_xml(export);

    len = strlen(EXPORT_ISCSI_FMT)
                 + UUID_STR_LEN
                 + strlen(lun)
                 + strlen(filter_policy)
                 + strlen(iqn_filters);
    contents = os_malloc(len + 1);

    EXA_ASSERT(os_snprintf(contents, len, EXPORT_ISCSI_FMT,
                UUID_VAL(export_get_uuid(export)),
                lun, filter_policy, iqn_filters)
               < len);

    os_free(iqn_filters);

    return contents;
}

char *lum_exports_to_xml(exa_error_code *err)
{
    char *contents;
    char version[16];
    int len, i;

    EXA_ASSERT(os_snprintf(version, sizeof(version), "%d", exports_version)
               < sizeof(version));

    len = strlen(XML_HEADER EXPORTLIST_START) + strlen(version);

    contents = os_malloc(len + 1);
    EXA_ASSERT(os_snprintf(contents, len, XML_HEADER EXPORTLIST_START,
                version) < len);

    EXA_ASSERT(err != NULL);
    *err = EXA_SUCCESS;

    for (i = 0; i < num_exports; i++)
    {
        char *export_str = NULL;

        switch (export_get_type(adm_exports[i]->desc))
        {
            case EXPORT_BDEV:
                export_str = lum_export_bdev_to_xml(adm_exports[i]->desc);
                break;
            case EXPORT_ISCSI:
                export_str = lum_export_iscsi_to_xml(adm_exports[i]->desc);
                break;
            default:
                EXA_ASSERT_VERBOSE(false, "Wrong export type");
        }

        if (export_str != NULL)
            contents = lum_strconcat(contents, export_str);

        os_free(export_str);
    }

    contents = lum_strconcat(contents, EXPORTLIST_END);

    return contents;
}

exa_error_code lum_serialize_exports(void)
{
    char path[OS_PATH_MAX];
    char *contents = NULL;
    exa_error_code r = EXA_SUCCESS;
    cl_error_desc_t err;

    contents = lum_exports_to_xml(&r);
    if (contents == NULL)
    {
        exalog_error("Failed dumping XML: %d", r);
        return r;
    }

    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADM_EXPORTS_FILE);

    adm_file_write_from_str(path, contents, &err);

    os_free(contents);
    return err.code;
}

bool lum_exports_remove_exports_file(void)
{
    char exports_file_path[OS_PATH_MAX];
    struct stat st;

    strlcpy(exports_file_path, lum_exports_get_exports_file_path(),
            sizeof(exports_file_path));

    if (stat(exports_file_path, &st) == 0)
        return unlink(exports_file_path) == 0;
    return true;
}

/**
 * @brief Get the export from its UUID.
 *
 * @param[in] uuid     The UUID of the export to get
 *
 * @return The export if found, NULL otherwise.
 */
static adm_export_t *lum_exports_get_export_by_uuid(const exa_uuid_t *uuid)
{
    int i;

    if (uuid == NULL)
        return NULL;

    for (i = 0; i < num_exports; i++)
    {
        if (uuid_is_equal(export_get_uuid(adm_exports[i]->desc), uuid))
            return adm_exports[i];
    }

    return NULL;
}

const adm_export_t *lum_exports_get_nth_export(int n)
{
    if (n < num_exports)
        return adm_exports[n];

    return NULL;
}

int lum_exports_get_number(void)
{
    return num_exports;
}

void lum_exports_clear(void)
{
    int i;

    for (i = 0; i < num_exports; i++)
        adm_export_free(adm_exports[i]);

    num_exports = 0;
}


/* XXX Returned info should include published status, ie based on
       adm_export_t, not export_t alone? */
int lum_exports_get_info(export_info_t **export_infos)
{
    int i;

    EXA_ASSERT(export_infos != NULL);

    *export_infos = os_malloc(num_exports * sizeof(export_info_t));
    if (*export_infos == NULL)
        return -ENOMEM;

    for (i = 0; i < num_exports; i++)
        export_get_info(adm_exports[i]->desc, *export_infos + i);

    return num_exports;
}

int lum_exports_serialize_export_by_uuid(const exa_uuid_t *uuid, char *buf,
                                         size_t buf_size)
{
    int nb;
    adm_export_t *adm_export = lum_exports_get_export_by_uuid(uuid);

    EXA_ASSERT(buf_size >= export_serialized_size());

    EXA_ASSERT(adm_export->desc != NULL);

    nb = export_serialize(adm_export->desc, buf, buf_size);

    return nb == export_serialized_size() ? EXA_SUCCESS : nb;
}

/* XXX Most of lum_create_export_iscsi() and lum_create_export_bdev()
       can be refactored */

int lum_create_export_iscsi(const exa_uuid_t *uuid, lun_t lun)
{
    adm_export_t *adm_export;
    export_t *export;
    int err;

    export = export_new_iscsi(uuid, lun, IQN_FILTER_DEFAULT_POLICY);
    if (export == NULL)
        return -ENOMEM;

    adm_export = adm_export_alloc();
    if (adm_export == NULL)
    {
        export_delete(export);
        return -ENOMEM;
    }
    adm_export_set(adm_export, export, false);

    err = lum_exports_insert_export(adm_export);
    if (err)
    {
        adm_export_free(adm_export);
        return err;
    }

    lum_exports_increment_version();

    err = lum_serialize_exports();
    if (err)
        return err;

    return EXA_SUCCESS;
}

int lum_create_export_bdev(const exa_uuid_t *uuid, const char *path)
{
    adm_export_t *adm_export;
    export_t *export;
    int err;

    /* major and minor will be 0 and are unused for now */
    export = export_new_bdev(uuid, path);
    if (export == NULL)
        return -ENOMEM;

    adm_export = adm_export_alloc();
    if (adm_export == NULL)
    {
        export_delete(export);
        return -ENOMEM;
    }
    adm_export_set(adm_export, export, false);

    err = lum_exports_insert_export(adm_export);
    if (err)
    {
        adm_export_free(adm_export);
        return err;
    }

    lum_exports_increment_version();

    err = lum_serialize_exports();
    if (err)
        return err;

    return EXA_SUCCESS;
}
