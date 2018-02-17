/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "admind/src/adm_license.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_version.h"
#include "common/include/uuid.h"
#include "os/include/strlcpy.h"
#include "os/include/os_mem.h"
#include "os/include/os_random.h"
#include "os/include/os_stdio.h"

ut_setup()
{
    os_random_init();
    UT_ASSERT_EQUAL(EXA_SUCCESS, adm_license_static_init());
}

ut_cleanup()
{
    os_random_cleanup();
    /* TODO Clean up */
}

ut_test(status_str_returns_different_string_for_each_status)
{
    /* This assumes the enum values are contiguous */
    const char *str[ADM_LICENSE_STATUS__LAST - ADM_LICENSE_STATUS__FIRST + 1];
    int i, j;

    for (i = ADM_LICENSE_STATUS__FIRST; i <= ADM_LICENSE_STATUS__LAST; i++)
    {
        str[i] = adm_license_status_str(i);
        for (j = ADM_LICENSE_STATUS__FIRST; j < i; j++)
            UT_ASSERT(strcmp(str[i], str[j]) != 0);
    }
}

ut_test(status_str_returns_null_for_invalid_status)
{
    UT_ASSERT(adm_license_status_str(ADM_LICENSE_STATUS__FIRST - 1) == NULL);
    UT_ASSERT(adm_license_status_str(ADM_LICENSE_STATUS__LAST + 1) == NULL);
}

/* Helper function used to generate xml formed buffers */
static char *generate_xml_license(const char *uuid, const char *licensee,
                                  const char *expiry, const char *product,
                                  const char *version, const char *type,
                                  const char *nodes, const char *ha,
                                  const char *maxsize)
{
    int ret;

    #define XML_LICENSE \
    "<?xml version=\"1.0\"?>"\
        "<license uuid=\"%s\""\
        "       licensee=\"%s\""\
        "       expiry=\"%s\""\
        "       product=\"%s\""\
        "       version=\"%s\""\
        "       type=\"%s\""\
        "       nodes=\"%s\""\
        "       ha=\"%s\""\
        "       maxsize=\"%s\"/>"

    static char xml[sizeof(XML_LICENSE)
           + UUID_STR_LEN
           + EXA_MAXSIZE_LICENSEE
           + 10 /* size of an expiry, we'll have to set 11 in 2286 */
           + 32 /* size of a product_name_t */
           + EXA_VERSION_LEN
           + 4 /* for now, 'full' or 'eval' */
           + 3 /* exanodes theorically supports 128 nodes*/
           + 3 /* 'yes' or 'no' for HA*/
           + 20 /* size is a uint64*/ ];

    UT_ASSERT(uuid != NULL && licensee != NULL && expiry != NULL && product != NULL
        && version != NULL && type != NULL && nodes != NULL && ha != NULL
        && maxsize != NULL);

    ret = os_snprintf(xml, sizeof(xml), XML_LICENSE, uuid, licensee, expiry, product,
                      version, type, nodes, ha, maxsize);

    UT_ASSERT(ret < sizeof(xml));

    return xml;
}

#define DATA "<?xml version=\"1.0\" ?>\r\n"\
"<!-- This license expires on 2076-09-05 -->\r\n"\
"<license uuid=\"FD4D084B:CBBC4E5D:A19A4DE6:0DE84F5A\"\r\n"\
"    type=\"full\"\r\n"\
"    licensee=\"github\"\r\n"\
"    expiry=\"3366489600\"\r\n"\
"    product=\"exanodes-hpc\"\r\n"\
"    version=\"5.0\"\r\n"\
"    nodes=\"400\"\r\n"\
"    ha=\"yes\"\r\n"\
"    maxsize=\"0\"/>\r\n"\

/* The data is "incorrect" wrt to the signature in the FOOTER below */
#define INCORRECT_DATA "<?xml version=\"1.0\" ?>\r\n"\
"<!-- This license expires on 2011-05-27 -->\r\n"\
"<license uuid=\"4D0456BD:29944A33:95736693:6C4BC253\"\r\n"\
"    licensee=\"Timothée\"\r\n"\
"    expiry=\"1306454400\"\r\n"\
"    product=\"exanodes-hpc\"\r\n"\
"    version=\"3.0\"\r\n"\
"    nodes=\"1024\"/>\r\n"\

#define HEADER "MIME-Version: 1.0\n"\
"Content-Type: multipart/signed; protocol=\"application/x-pkcs7-signature\"; micalg=\"sha1\"; boundary=\"----39AE5A3F57D0CEEADD3BAAEF65383438\"\n"\
"\n"\
"This is an S/MIME signed message\n"\
"\n"\
"------39AE5A3F57D0CEEADD3BAAEF65383438\n"\

#define INCORRECT_HEADER "MIME-Version: 1.0\n"\
"Content-Type: multipart/signed; protocol=\"application/x-pkcs7-signature\"; micalg=sha1; boundary=\"----94006836B2324AxxxxxxxxxxxxxxxxxxxxxxxxECDE28A3ACD1B8E6FA\"\n"\
"\n"\
"This is an S/MIME signed message\n"\
"\n"\
"------94006836B2324AECDE28A3ACD1B8E6FA\n"\

#define FOOTER "\n"\
"------39AE5A3F57D0CEEADD3BAAEF65383438\n"\
"Content-Type: application/x-pkcs7-signature; name=\"smime.p7s\"\n"\
"Content-Transfer-Encoding: base64\n"\
"Content-Disposition: attachment; filename=\"smime.p7s\"\n"\
"\n"\
"MIIEMgYJKoZIhvcNAQcCoIIEIzCCBB8CAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3\n"\
"DQEHAaCCAhgwggIUMIIBfQIBATANBgkqhkiG9w0BAQQFADBcMREwDwYDVQQKEwhT\n"\
"ZWFub2RlczEMMAoGA1UECxQDUiZEMREwDwYDVQQHEwhUb3Vsb3VzZTELMAkGA1UE\n"\
"BhMCRlIxGTAXBgNVBAMTEHd3dy5zZWFub2Rlcy5jb20wHhcNMTUwODA0MDk1ODA0\n"\
"WhcNNDIxMjIwMDk1ODA0WjBJMQswCQYDVQQGEwJGUjERMA8GA1UEChMIU2Vhbm9k\n"\
"ZXMxDDAKBgNVBAsUA1ImRDEZMBcGA1UEAxMQd3d3LnNlYW5vZGVzLmNvbTCBnzAN\n"\
"BgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAspZNS3yl++OAYw/kSLnYceuwb5yH/4HW\n"\
"WGz3pmtaONSD91gA9dloLKDpidhTBo1Vtf4Ve9Hfp3qkcplesClzNWMsKDVfXm6D\n"\
"3WRxy3DgbokUZAALFQEO770IBf/WmhMIjPvt9On4LMgU563Jg70lsF6yfkl7+hPb\n"\
"iowAP8LUXEcCAwEAATANBgkqhkiG9w0BAQQFAAOBgQBktbxKnwqMhWuIYWhyUOei\n"\
"ehweZ/ELtyPV1mbFTdQpYtqlE8fkH8dmCt7XxMbbO5Djkwq1hDA1iadMq3lRc6pY\n"\
"2NJGd45oIy3jiE8Ax1CnYbffHqHYgRR0YEWvM4ZvgtPzd1cmB5IFuzeIG7rpOkw1\n"\
"6gRBJbvMtd+JUfbvjMztwzGCAeIwggHeAgEBMGEwXDERMA8GA1UEChMIU2Vhbm9k\n"\
"ZXMxDDAKBgNVBAsUA1ImRDERMA8GA1UEBxMIVG91bG91c2UxCzAJBgNVBAYTAkZS\n"\
"MRkwFwYDVQQDExB3d3cuc2Vhbm9kZXMuY29tAgEBMAkGBSsOAwIaBQCggdgwGAYJ\n"\
"KoZIhvcNAQkDMQsGCSqGSIb3DQEHATAcBgkqhkiG9w0BCQUxDxcNMTUwOTA1MTYz\n"\
"MTU5WjAjBgkqhkiG9w0BCQQxFgQU4Cs/vtCxCD/02Bj/+JMh68SntDYweQYJKoZI\n"\
"hvcNAQkPMWwwajALBglghkgBZQMEASowCwYJYIZIAWUDBAEWMAsGCWCGSAFlAwQB\n"\
"AjAKBggqhkiG9w0DBzAOBggqhkiG9w0DAgICAIAwDQYIKoZIhvcNAwICAUAwBwYF\n"\
"Kw4DAgcwDQYIKoZIhvcNAwICASgwDQYJKoZIhvcNAQEBBQAEgYAY4Pjy1r6L3YG5\n"\
"Rw6wmMYnGbnzZ4h+hWUxnvaKj/HVLpaRAyJ9nxcSnOTQ4q+rHgmN8fg/2baOMhL2\n"\
"PXrXiBke8vCe2GgjIQYQrWxDXrlyJ96oEbKS83h0G39V97KW2AyIktNv6IWASqHj\n"\
"ytiwqdlapHK9dZt0RJXGp5Dw41jDuQ==\n"\
"\n"\
"------39AE5A3F57D0CEEADD3BAAEF65383438--\n"\
"\n"

ut_test(license_uncypher_data_with_correct_data_returns_readable)
{
    cl_error_desc_t error_desc;

    int size_str = strlen(HEADER) + strlen(DATA) + strlen(FOOTER) + 1;
    char cyphered_buf[size_str];
    char *readable;

    UT_ASSERT(
        os_snprintf(cyphered_buf, size_str, "%s%s%s", HEADER, DATA, FOOTER)
        < size_str);

    readable = adm_license_uncypher_data(cyphered_buf, strlen(cyphered_buf),
                                         &error_desc);

    if (error_desc.code != EXA_SUCCESS)
        ut_printf("%s", error_desc.msg);

    UT_ASSERT_EQUAL_STR(readable, DATA);
    os_free(readable);
}

ut_test(license_uncypher_data_with_incorrect_data_returns_NULL)
{
    cl_error_desc_t error_desc;

    int size_str = strlen(HEADER) + strlen(INCORRECT_DATA) + strlen(FOOTER) + 1;
    char cyphered_buf[size_str];

    UT_ASSERT(
        os_snprintf(cyphered_buf, size_str, "%s%s%s", HEADER, INCORRECT_DATA, FOOTER)
        < size_str);

    UT_ASSERT(
        adm_license_uncypher_data(cyphered_buf, strlen(cyphered_buf), &error_desc)
        == NULL);
}

ut_test(license_uncypher_data_with_incorrect_header_returns_NULL)
{
    cl_error_desc_t error_desc;

    int size_str = strlen(INCORRECT_HEADER) + strlen(DATA) + strlen(FOOTER) + 1;
    char cyphered_buf[size_str];

    UT_ASSERT(
        os_snprintf(cyphered_buf, size_str, "%s%s%s", INCORRECT_HEADER, DATA, FOOTER)
        < size_str);

    UT_ASSERT(
        adm_license_uncypher_data(cyphered_buf, strlen(cyphered_buf), &error_desc)
        == NULL);
}
ut_test(xml_license_parsing_return_valid_license)
{
    cl_error_desc_t error_desc;
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "5",
        "yes", "1000");

    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));
    exa_uuid_t uuid;

    uuid_scan("20524188:2A2F4B01:52D56CF5:28017CA9", &uuid);

    UT_ASSERT(license != NULL);
    UT_ASSERT(!adm_license_nb_nodes_ok(license, 115, &error_desc));
    UT_ASSERT(adm_license_nb_nodes_ok(license, 5, &error_desc));
    UT_ASSERT(uuid_is_equal(adm_license_get_uuid(license), &uuid));
    UT_ASSERT(adm_license_is_eval(license));
    UT_ASSERT(adm_license_has_ha(license));
    UT_ASSERT(adm_license_size_ok(license, 500, &error_desc));
    UT_ASSERT(!adm_license_size_ok(license, 49954, &error_desc));

    adm_license_delete(license);
}

ut_test(zero_nodes_prevents_any_cluster)
{
    cl_error_desc_t error_desc;
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "0",
        "yes", "1000");
        adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(!adm_license_nb_nodes_ok(license, 1, &error_desc));

    adm_license_delete(license);
}

ut_test(cluster_nb_nodes_less_than_license_nb_nodes_is_ok)
{
    cl_error_desc_t error_desc;
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(adm_license_nb_nodes_ok(license, 2, &error_desc));

    adm_license_delete(license);
}

ut_test(cluster_nb_nodes_more_than_license_nb_nodes_is_not_ok)
{
    cl_error_desc_t error_desc;
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(!adm_license_nb_nodes_ok(license, 42, &error_desc));

    adm_license_delete(license);
}

ut_test(license_get_size)
{
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT_EQUAL(1000, adm_license_get_max_size(license));

    adm_license_delete(license);
}

ut_test(correct_license_type_is_ok)
{
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(license != NULL);

    adm_license_delete(license);
}

ut_test(wrong_license_type_is_not_ok)
{
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "plop", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(license == NULL);

    adm_license_delete(license);
}

ut_test(get_status_returns_ok_when_expiry_date_in_future)
{
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "9306454400", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(adm_license_get_status(license) == ADM_LICENSE_STATUS_OK);

    adm_license_delete(license);
}

ut_test(get_status_returns_expired_when_grace_period_past)
{
    char *xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", "1111111111", "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    adm_license_t *license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT(adm_license_get_status(license) == ADM_LICENSE_STATUS_EXPIRED);

    adm_license_delete(license);
}

ut_test(new_from_xml_various_xml_errors)
{
    char xml[8192];
    struct linetest
    {
       char *p1[3];
    };
    typedef struct linetest linetest_t;

    linetest_t test[11] = {
        {{"<license ",  "<license ",   " "}},
        {{" uuid=\"4D0456BD:29944A33:95736693:6C4BC253\"\r\n",
          " uuid=\"4____________944A33:95736693:6C4BC253\"\r\n",
          " "}},
        {{" licensee=\"Sebastien\"\r\n",
          " licensee=\"AZDFEZEFZEDFZEGERZEGZERGZERGFZEKOZE4FZERGFPZERTGKERGGFKZERGPKZERGPERKGERPGKERPGKZAEF.PZEV.EZPVZEPF.ZEPFKZEPFEFKZEPFKAPEFKEPFKEAFKPEPKFPEKAFAPKFE\"\r\n",
          " "}},
        {{" expiry=\"1306454400\"\r\n",
          " expiry=\"AZEFEZFZERGR REGR GR RG E\"\r\n",
          " "}},
        {{" product=\"exanodes-hpc\"\r\n",
          " product=\"eexanexanexanexanexanexanexanexanexanexanxanodes-hpc\"\r\n",
          " "}},
        {{" version=\"3.0\"\r\n",
          " version=\"X.9434.ARFETF342.preseles/beta-ac.mm\"\r\n",
          " "}},
        {{" nodes=\"4\"",
          " nodes=\"-16359841548756444144444444444444585212\"",
          " "}},
        {{" type=\"eval\"",
          " type=\"0ve|210r|)\"",
          " "}},
        {{" ha=\"yes\"",
          " ha=\"saucisse\"",
          " "}},
        {{" maxsize=\"1000\"",
          " type=\"-122G\"",
          " "}},
        {{" />\r\n",
          " ///<<><>\r\n",
          " "}},
    };

    adm_license_t *license;
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);
    license = adm_license_new_from_xml(xml, sizeof(xml));

    UT_ASSERT(license != NULL);

    os_free(license);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[1], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);
    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[1],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[1], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[1], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[1],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[1], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[1], test[8].p1[0],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[1],
                test[9].p1[0], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[1], test[10].p1[0]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

    memset(&xml, 0, 8192);
    os_snprintf(xml, sizeof(xml), "%s %s %s %s %s %s %s %s %s %s %s",
                test[0].p1[0], test[1].p1[0], test[2].p1[0],
                test[3].p1[0], test[4].p1[0], test[5].p1[0],
                test[6].p1[0], test[7].p1[0], test[8].p1[0],
                test[9].p1[0], test[10].p1[1]);

    UT_ASSERT(adm_license_new_from_xml(xml, sizeof(xml)) == NULL);

}

static void __check_status(time_t expiry, adm_license_status_t expected_status)
{
    char expiry_str[32];
    char *xml;
    adm_license_t *license;
    adm_license_status_t status;

    UT_ASSERT(os_snprintf(expiry_str, sizeof(expiry_str), "%lu", expiry)
              < sizeof(expiry_str));

    xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
                               "NAME", expiry_str, "exanodes-" EXA_EDITION_TAG,
                               "3.0", "eval", "5", "yes", "1000");

    license = adm_license_new_from_xml(xml, strlen(xml));
    UT_ASSERT(license != NULL);

    status = adm_license_get_status(license);
    UT_ASSERT_VERBOSE(status == expected_status,
                      "expected '%s', got '%s'",
                      adm_license_status_str(expected_status),
                      adm_license_status_str(status));

    adm_license_delete(license);
}

ut_test(get_status_expired_grace_ok)
{
    time_t now, expiry;
    unsigned days;

    /* Start with an expiry one day older than today minus the grace period */
    time(&now);
    UT_ASSERT(now != (time_t)-1);
    expiry = now - (ADM_LICENSE_GRACE_PERIOD + 1) * 24 * 3600;

    __check_status(expiry, ADM_LICENSE_STATUS_EXPIRED);

    for (days = 1; days <= ADM_LICENSE_GRACE_PERIOD; days++)
    {
        expiry += 24 * 3600;
        __check_status(expiry, ADM_LICENSE_STATUS_GRACE);
    }

    expiry += 24 * 3600;
    __check_status(expiry, ADM_LICENSE_STATUS_OK);
}

ut_test(new_returns_not_NULL)
{
    adm_license_t *license;
    exa_uuid_t uuid;
    struct tm expires;

    uuid_generate(&uuid);

   /* we put some crust in expires as it isn't verified (yet)*/
   memset(&expires, 0xEE, sizeof(expires));

   license = adm_license_new("plop", &uuid, expires, "exa", 2, "full");
   UT_ASSERT(license != NULL);
   adm_license_delete(license);
}

ut_test(get_remaining_days_in_grace_returns_remaining_days)
{
    char *xml;
    char expiry_str[32];
    adm_license_t *license;
    time_t expiry;

    /* first and second half of grace delay, in days*/
    int grace1, grace2, day;
    day = 24 * 3600;

    grace1 = ADM_LICENSE_GRACE_PERIOD / 2;
    grace2 = ADM_LICENSE_GRACE_PERIOD - grace1 ;

    UT_ASSERT_EQUAL(ADM_LICENSE_GRACE_PERIOD, grace1 + grace2);

    /* We set an expiration date `half the grace delay` ago,
     * so we are in grace period
    */
    expiry = time(NULL) - grace1 * day;

    UT_ASSERT(os_snprintf(expiry_str, sizeof(expiry_str), "%lu", expiry)
              < sizeof(expiry_str));

    xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", expiry_str, "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT_EQUAL(grace2, adm_license_get_remaining_days(license, true));

    adm_license_delete(license);
}

ut_test(get_remaining_days_returns_remaining_days)
{
    char *xml;
    char expiry_str[32];
    adm_license_t *license;
    time_t expiry;
    int day = 24 * 3600;

    /* We set an expiration date in 20 days from now */
    expiry = time(NULL) + 20 * day;

    UT_ASSERT(os_snprintf(expiry_str, sizeof(expiry_str), "%lu", expiry)
              < sizeof(expiry_str));

    xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", expiry_str, "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT_EQUAL(20 + ADM_LICENSE_GRACE_PERIOD,
                    adm_license_get_remaining_days(license, true));

    adm_license_delete(license);
}

ut_test(get_remaining_days_when_expired_returns_0)
{
    char *xml;
    char expiry_str[32];
    adm_license_t *license;
    time_t expiry;
    int day = 24 * 3600;

    /* We set an expiration date a too long time ago */
    expiry = time(NULL) - 2 * ADM_LICENSE_GRACE_PERIOD * day;

    UT_ASSERT(os_snprintf(expiry_str, sizeof(expiry_str), "%lu", expiry)
              < sizeof(expiry_str));

    xml = generate_xml_license("20524188:2A2F4B01:52D56CF5:28017CA9",
        "NAME", expiry_str, "exanodes-" EXA_EDITION_TAG, "3.0", "eval", "3",
        "yes", "1000");
    license = adm_license_new_from_xml(xml, strlen(xml));

    UT_ASSERT_EQUAL(0, adm_license_get_remaining_days(license, true));

    adm_license_delete(license);
}

ut_test(deserialize_with_incorrect_path_returns_NULL)
{
    cl_error_desc_t err_desc;
    char *badpath="/reo/gerger/Gerg/gerG/er/Gerg.fzefzef234FE";

    UT_ASSERT(adm_deserialize_license(badpath, &err_desc) == NULL);
}
