/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <libxml/parser.h>

#include "admind/src/adm_license.h"
#include "admind/src/adm_node.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "common/include/exa_version.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "common/include/uuid.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"
#include "admind/src/adm_file_ops.h"
#include "admind/src/cacert.h"

/*
 * XXX TODO Implement X509 certificate chain and signature verification
 * on Windows
 */
#include <openssl/ssl.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/err.h>
static X509_STORE *store = NULL;

/*
 * XXX TODO Check this whole thing doesn't leak memory!
 */

typedef char product_name_t[32];

struct adm_license
{
    char licensee[EXA_MAXSIZE_LICENSEE + 1];
    exa_uuid_t uuid;
    struct tm expiry;
    bool is_eval;
    product_name_t product_name;
    exa_version_t major_version;
    uint32_t max_nodes;
    bool has_ha;
    uint64_t max_size;
};

const exa_uuid_t *adm_license_get_uuid(const adm_license_t *license)
{
    EXA_ASSERT(license != NULL);

    return &license->uuid;
}

/* Time until the license expires. May be negative in case it has already
   expired (not taking into account the grace period). */
static double time_to_expiry(const adm_license_t *license)
{
    time_t expiry;
    time_t now;
    struct tm license_expiry;

    EXA_ASSERT(license != NULL);

    license_expiry = license->expiry;
    expiry = mktime(&license_expiry);

    time(&now);

    return difftime(expiry, now);
}

bool adm_license_is_eval(const adm_license_t *license)
{
    EXA_ASSERT(license != NULL);
    return license->is_eval;
}

bool adm_license_has_ha(const adm_license_t *license)
{
    EXA_ASSERT(license != NULL);
    return license->has_ha;
}

uint64_t adm_license_get_max_size(const adm_license_t *license)
{
    EXA_ASSERT(license != NULL);
    return license->max_size;
}

adm_license_status_t adm_license_get_status(const adm_license_t *license)
{
    /* remaining days to expiration */
    double diff = time_to_expiry(license) / 24. / 3600. ;

    if (diff >= 0)
	return ADM_LICENSE_STATUS_OK;
    else if (diff >= -ADM_LICENSE_GRACE_PERIOD)
	return ADM_LICENSE_STATUS_GRACE;
    else
	return ADM_LICENSE_STATUS_EXPIRED;
}

unsigned int adm_license_get_remaining_days(const adm_license_t *license,
                                            bool grace_period)
{
    /* remaining days to expiration */
    double diff = time_to_expiry(license) / 24. / 3600. ;

    if (grace_period)
	diff += ADM_LICENSE_GRACE_PERIOD;

    if (diff >= 0)
	return (unsigned int)diff;
    else
	return 0;
}

bool adm_license_nb_nodes_ok(const adm_license_t *license, uint32_t nb,
                             cl_error_desc_t *error_desc)
{
    EXA_ASSERT(license != NULL);

    if (nb > license->max_nodes)
    {
        set_error(error_desc, -ADMIND_ERR_LICENSE,
                  "The license is limited to %u nodes.", license->max_nodes);
	return false;
    }

    set_success(error_desc);
    return true;
}

bool adm_license_size_ok(const adm_license_t *license, uint64_t size,
                         cl_error_desc_t *error_desc)
{
    char maxsize_tib[16];
    EXA_ASSERT(license != NULL);
    if (size > license->max_size)
    {
        EXA_ASSERT(exa_get_human_size(maxsize_tib, strlen(maxsize_tib),
                   license->max_size) != NULL);
        set_error(error_desc, -ADMIND_ERR_LICENSE,
                  "The license is limited in size to %s", maxsize_tib);
        return false;
    }

    set_success(error_desc);
    return true;
}


bool adm_license_matches_self(const adm_license_t *license,
                              cl_error_desc_t *error_desc)
{
    exa_version_t major_version_running;
    EXA_ASSERT(license);

    EXA_ASSERT_VERBOSE(exa_version_get_major(EXA_VERSION, major_version_running),
                       "failed getting major version of '%s'", EXA_VERSION);

    /* XXX Should use symbolic constant instead of 'exanodes' */
    if (strcmp(license->product_name, "exanodes-" EXA_EDITION_TAG) != 0)
    {
        set_error(error_desc, -ADMIND_ERR_LICENSE,
                  "License is for product '%s', expected '%s'",
                  license->product_name, "exanodes-" EXA_EDITION_TAG);
        return false;
    }

    if (!exa_version_is_equal(license->major_version, major_version_running))
    {
        set_error(error_desc, -ADMIND_ERR_LICENSE,
                  "License is for version '%s', expected '%s'",
                  license->major_version, major_version_running);
        return false;
    }

    return true;
}

/* Set the license fields from the XML properties, validating them in the
   process. */
static bool adm_license_set_license_from_nodeptr(xmlNodePtr node,
                                                 adm_license_t *license)
{
    time_t t;
    const char *str;

    str = xml_get_prop(node, "uuid");
    if (str == NULL || uuid_scan(str, &license->uuid) != EXA_SUCCESS)
        return false;

    str = xml_get_prop(node, "licensee");
    if (str == NULL
        || strlcpy(license->licensee, str, sizeof(license->licensee))
               >= sizeof(license->licensee))
        return false;

    str = xml_get_prop(node, "product");
    if (str == NULL
        || strlcpy(license->product_name, str, sizeof(license->product_name))
               >= sizeof(license->product_name))
        return false;

    str = xml_get_prop(node, "type");
    if (str == NULL)
        return false;

    /* the string must match eval or full, another value will be denied */
    if (strcmp(str, "eval") == 0)
        license->is_eval = true;
    else if(strcmp(str, "full") == 0)
        license->is_eval = false;
    else
        return false;

    str = xml_get_prop(node, "expiry");
    if (str == NULL)
        return false;

    /* FIXME Handle 'never'. */
    /* FIXME Write to_time() */

    /* This is true on Linux and on 64 bit Windows, but not on 32 bit
       Windows, where time_t is 32 bits wide. */
    COMPILE_TIME_ASSERT(sizeof(t) == sizeof(int64_t));

    if (to_int64(str, &t) != 0 || !os_localtime(&t, &license->expiry))
        return false;

    str = xml_get_prop(node, "version");
    if (str == NULL
        || exa_version_from_str(license->major_version, str) != EXA_SUCCESS
        || !exa_version_is_major(license->major_version))
        return false;

    str = xml_get_prop(node, "nodes");
    if (str == NULL || to_uint32(str, &license->max_nodes) != 0)
        return false;

    str = xml_get_prop(node, "ha");
    if (str == NULL)
        return false;

    /* The string must match yes or no */
    if (strcmp(str, "yes") == 0)
        license->has_ha = true;
    else if (strcmp(str, "no") == 0)
        license->has_ha = false;
    else
        return false;

    str = xml_get_prop(node, "maxsize");
    if (str == NULL || to_uint64(str, &license->max_size) != 0)
        return false;

    return true;
}

adm_license_t *adm_license_new_from_xml(const char *buffer, size_t len)
{
    adm_license_t *license;
    xmlDocPtr doc = xml_conf_init_from_buf(buffer, len);
    xmlNodePtr node;

    if (doc == NULL)
        return NULL;

    license = (adm_license_t *)os_malloc(sizeof(adm_license_t));
    if (license == NULL)
    {
        xmlFreeDoc(doc);
        return NULL;
    }

    node = xml_conf_xpath_singleton(doc, "//license");

    if (node == NULL)
    {
        xmlFreeDoc(doc);
        os_free(license);
        return NULL;
    }

    if (!adm_license_set_license_from_nodeptr(node, license))
    {
        os_free(license);
        license = NULL;
    }

    xmlFreeDoc(doc);

    return license;
}

char *adm_license_uncypher_data(char *cypher, size_t size, cl_error_desc_t *error_desc)
{
    char *str;
    char *buffer;
    long buf_size;
    STACK_OF(X509) *chain = NULL;
    BIO *bio, *pb = NULL;
    PKCS7 *pkcs7;

    if ((bio = BIO_new_mem_buf(cypher, size)) == NULL)
    {
	set_error(error_desc, -ENOMEM, "Error creating BIO verification object.");
        return NULL;
    }

    if ((pkcs7 = SMIME_read_PKCS7(bio, &pb)) == NULL)
    {
	set_error(error_desc, -ADMIND_ERR_LICENSE,
                  "Error reading PKCS#7 object: %s",
                  ERR_reason_error_string(ERR_get_error()));
	BIO_free(bio);
        return NULL;
    }

    if (PKCS7_verify(pkcs7, chain, store, pb, NULL, 0) != 1)
    {
	set_error(error_desc, -ADMIND_ERR_LICENSE,
                  "Error verifying license: %s",
                  ERR_reason_error_string(ERR_get_error()));
        PKCS7_free(pkcs7);
	BIO_free(bio);
        BIO_free(pb);
        return NULL;
    }

    buf_size = BIO_get_mem_data(pb, &buffer);

    str = os_strndup(buffer, buf_size);

    PKCS7_free(pkcs7);
    BIO_free(bio);
    BIO_free(pb);

    set_success(error_desc);

    return str;
}

adm_license_t *adm_deserialize_license(const char *path,
                                       cl_error_desc_t *err_desc)
{
    adm_license_t *license = NULL;
    char *buffer, *xml_str;

    buffer = adm_file_read_to_str(path, err_desc);
    if (buffer == NULL)
        return NULL;

    xml_str = adm_license_uncypher_data(buffer, strlen(buffer) + 1, err_desc);

    os_free(buffer);

    if (xml_str == NULL)
        return NULL;

    license = adm_license_new_from_xml(xml_str, strlen(xml_str));

    os_free(xml_str);

    if (license == NULL)
    {
	set_error(err_desc, -EXA_ERR_XML_PARSE, "Failed to parse XML license data.");
        return NULL;
    }

    set_success(err_desc);

    return license;
}

/* SSL verification callback */
static int ssl_verify_cb(int success, X509_STORE_CTX *store)
{
    if (success == 0)
    {
	char data[256];
	X509 *cert = X509_STORE_CTX_get_current_cert(store);
	int depth = X509_STORE_CTX_get_error_depth(store);
	int err = X509_STORE_CTX_get_error(store);

	exalog_error("Error with certificate at depth: %i", depth);
	X509_NAME_oneline(X509_get_issuer_name(cert), data, sizeof data);
	exalog_error(" issuer = %s", data);
	X509_NAME_oneline(X509_get_subject_name(cert), data, sizeof data);
	exalog_error(" subject = %s", data);
	exalog_error(" err %i: %s", err, X509_verify_cert_error_string(err));
	return 0;
    }
    return 1;
}

int adm_license_static_init(void)
{
    const unsigned char *p = cert;
    int len = sizeof(cert);
    X509 *x;
    int ret;

    /* Init libssl */

    SSL_library_init();
    SSL_load_error_strings();

    if ((store = X509_STORE_new()) == NULL)
    {
	exalog_error("Error creating X509 store: %s",
                     ERR_reason_error_string(ERR_get_error()));
	return -ADMIND_ERR_INIT_SSL;
    }

    X509_STORE_set_verify_cb_func(store, ssl_verify_cb);

    x = d2i_X509(NULL, &p, len);
    if (x == NULL)
    {
	exalog_error("Error loading CA cert: %s",
                     ERR_reason_error_string(ERR_get_error()));
	return -ADMIND_ERR_INIT_SSL;
    }

    if ((ret = X509_STORE_add_cert(store, x)) != 1)
    {
	exalog_error("Error adding CA cert: %s",
                     ERR_reason_error_string(ERR_get_error()));
	return -ADMIND_ERR_INIT_SSL;
    }

    return EXA_SUCCESS;
}


adm_license_t *adm_license_install(const char *data, size_t size,
                                   cl_error_desc_t *error_desc)
{
    char path[OS_PATH_MAX];
    char path_tmp[OS_PATH_MAX];
    adm_license_t *license;
    int count;
    FILE *fp;

    EXA_ASSERT(error_desc != NULL);

    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADM_LICENSE_FILE);
    exa_env_make_path(path_tmp, sizeof(path_tmp), exa_env_cachedir(), ADM_LICENSE_FILE ".tmp");

    /* Write the license to the hard disk */

    if ((fp = fopen(path_tmp, "w")) == NULL)
    {
	set_error(error_desc, -errno, "Cannot create license file: %s (%d)",
                  exa_error_msg(-errno), -errno);
	return NULL;
    }

    count = fwrite(data, sizeof(char), size, fp);
    fclose(fp);

    if (count != size)
    {
	set_error(error_desc, -errno, "Cannot write license file: %s (%d)",
                  exa_error_msg(-errno), -errno);
	return NULL;
    }

    /* Parse the new license */

    license = adm_deserialize_license(path_tmp, error_desc);

    if (license == NULL
        || !adm_license_matches_self(license, error_desc))
    {
        adm_license_delete(license);
        return NULL;
    }

    os_file_rename(path_tmp, path);

    exalog_info("Successfully installed %s license with UUID "UUID_FMT" (status: %s).",
                    adm_license_is_eval(license) ? "evaluation":"full",
                    UUID_VAL(adm_license_get_uuid(license)),
                    adm_license_status_str(
                        adm_license_get_status(license)));
    return license;
}


void adm_license_uninstall(adm_license_t *license)
{
    char path[OS_PATH_MAX];

    exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADM_LICENSE_FILE);
    adm_license_delete(license);
    unlink(path);
}

adm_license_t *adm_license_new(const char *licensee, const exa_uuid_t *uuid,
                               struct tm expiry, const exa_version_t major_version,
                               uint32_t max_nodes, bool is_eval)
{
    adm_license_t *license;

    EXA_ASSERT(licensee != NULL);
    EXA_ASSERT(uuid != NULL);

    license = (adm_license_t *)os_malloc(sizeof(adm_license_t));
    if (license == NULL)
        return NULL;

    memset(license, 0xEE, sizeof(adm_license_t));

    strlcpy(license->licensee, licensee, sizeof(license->licensee));
    uuid_copy(&license->uuid, uuid);
    license->expiry = expiry;
    exa_version_copy(license->major_version, major_version);
    license->max_nodes = max_nodes;
    license->is_eval = is_eval;

    return license;
}

void adm_license_delete(adm_license_t *license)
{
    if (license != NULL)
        os_free(license);
}

const char *adm_license_status_str(adm_license_status_t status)
{
    if (ADM_LICENSE_STATUS_IS_VALID(status))
        switch (status)
        {
        case ADM_LICENSE_STATUS_NONE: return "(none)";
        case ADM_LICENSE_STATUS_EXPIRED: return "expired";
        case ADM_LICENSE_STATUS_GRACE: return "grace";
        case ADM_LICENSE_STATUS_EVALUATION: return "eval";
        case ADM_LICENSE_STATUS_OK: return "ok";
        }

    return NULL;
}

const char *adm_license_type_str(adm_license_t *license)
{
    if (license == NULL)
        return NULL;

    return license->is_eval ? "eval":"full";
}
