/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "config/exa_version.h"
#include "common/include/exa_error.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_deserialize.h"
#include "admind/src/adm_hostname.h"

#include "os/include/os_stdio.h"
#include "os/include/os_file.h"

#include <unit_testing.h>

#ifdef WIN32
#define putenv _putenv
#endif

static const char *test_conf_format =
"<?xml version=\"1.0\"?>\n\
<Exanodes release=\"%s\">\n\
  <cluster name=\"%s\" uuid=\"12345678:12345678:12345678:12345678\">\n\
    <node name=\"%s\" hostname=\"%s\" number=\"0\">\n\
      <network hostname=\"1.2.3.4\"/>\n\
      <disk uuid=\"12345678:12345678:12345678:12345678\" path=\"%s\"/>\n\
    </node>\n\
    <node name=\"node_local\" hostname=\"%s\" number=\"1\">\n\
      <network hostname=\"127.0.0.1\"/>\n\
    </node>\n\
  </cluster>\n\
</Exanodes>";


/**
 * Dummy function to help linking.
 */
char **fs_get_definition(void)
{
    static char * names[] = { "ext3", "sfs", NULL };
    return names;
}


typedef struct
{
    const char *cluster_name;
    const char *node_name;
    const char *host_name;
    const char *disk_path;
} names_to_check_t;


static int check_name(names_to_check_t* names)
{
    char test_conf[1024];
    char error_msg[EXA_MAXSIZE_LINE + 1];
    int res;

    putenv("EXANODES_NODE_CONF_DIR=" OS_FILE_SEP "does_not_exist");

    adm_cluster_cleanup();

    os_snprintf(test_conf, sizeof(test_conf), test_conf_format, EXA_VERSION,
		names->cluster_name, names->node_name, names->host_name,
		names->disk_path, adm_hostname());

    res = adm_deserialize_from_memory(test_conf, strlen(test_conf),
                                      error_msg, true);

    if (res != EXA_SUCCESS)
        ut_printf("check_name(cluster:\"%s\", node:\"%s\", host:\"%s\", disk:\"%s\") failed: %s",
                names->cluster_name, names->node_name, names->host_name,
                names->disk_path, error_msg);

    return res;
}


ut_setup()
{
    char dir_name[] = "/tmp/Exanodes_XXXXXX";
    char *res = mkdtemp(dir_name);
    UT_ASSERT(res != NULL);
    setenv("EXANODES_CACHE_DIR", dir_name, 0);
    adm_cluster_init();
}


ut_cleanup()
{
    adm_cluster_cleanup();
}

/* FIXME There should be many more unit tests to check each and every field.
 * Granted, adm_deserialize()'s API is not easily testable... */

ut_test(ok_1)
{
    names_to_check_t names =
        {
            .cluster_name = "Cluster_0.9-z",
            .node_name = "Node_0.9-z",
            .host_name = "Host_0123456789-xyz.toulouse",
#ifdef WIN32
            .disk_path = "\\\\?\\E:"
#else
            .disk_path = "/dev/sys/block/sdb/device/scsi_device:1:0:0:0"
#endif
        };
    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name(&names));
}

ut_test(ok_2)
{
    names_to_check_t names =
    {
        .cluster_name = "a_0.9-z",
        .node_name = "a_0.9-z",
        .host_name = "a_0.9-z",
#ifdef WIN32
            .disk_path = "\\\\?\\E:"
#else
        .disk_path = "/dev/a:b_0.9-z"
#endif
    };
    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name(&names));
}

ut_test(invalid_device_returns_error)
{
    names_to_check_t names =
    {
        .cluster_name = "a_0.9-z",
        .node_name = "a_0.9-z",
        .host_name = "a_0.9-z",
#ifdef WIN32
            .disk_path = "toto:"
#else
        .disk_path = "/toto/a:b_0.9-z"
#endif
    };
    UT_ASSERT(EXA_SUCCESS != check_name(&names));
}

ut_test(invalid_hostname_returns_error)
{
    names_to_check_t names =
    {
        .cluster_name = "a_0.9-z",
        .node_name = "a_0.9-z",
        .host_name = "a_0.:9-z",
#ifdef WIN32
	.disk_path = "\\\\?\\E:"
#else
        .disk_path = "/dev/a:b_0.9-z"
#endif
    };
    UT_ASSERT(EXA_SUCCESS != check_name(&names));
}

ut_test(longest_possible_hostname_succeeds)
{
    names_to_check_t names =
    {
        .cluster_name = "123456789ABCDEF",
        .node_name = "a_0.9-z",
        .host_name = "a_0.9-z",
#ifdef WIN32
	.disk_path = "\\\\?\\E:"
#else
        .disk_path = "/dev/a:b_0.9-z"
#endif
    };
    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name(&names));
    UT_ASSERT_EQUAL_STR(names.cluster_name, adm_cluster.name);
}

ut_test(too_long_cluster_name_returns_error)
{
    names_to_check_t names =
    {
        .cluster_name = "0123456789ABCDEF",
        .node_name = "a_0.9-z",
        .host_name = "a_0.9-z",
#ifdef WIN32
	.disk_path = "\\\\?\\E:"
#else
        .disk_path = "/a:b_0.9-z"
#endif
    };
    UT_ASSERT(EXA_SUCCESS != check_name(&names));
}

static const char *const test_conf_bad_group_format =
    "<?xml version=\"1.0\"?>\n\
    <Exanodes release=\"%s\" config_version=\"1\" format_version=\"1\" >\n\
       <diskgroup %s %s tainted=\"FALSE\" goal=\"STOPPED\" transaction=\"COMMITTED\" uuid=\"12345678:12345678:12345678:12345678\" sb_version=\"33\">\n\
       </diskgroup>\n\
       <cluster name=\"cluster\" uuid=\"12345678:12345678:12345678:12345678\">\n\
           <node name=\"Node\" hostname=\"%s\" number=\"0\">\n\
               <network hostname=\"1.2.3.4\"/>\n\
               <disk uuid=\"12345678:12345678:12345678:12345678\"/>\n\
           </node>\n\
       </cluster>\n\
    </Exanodes>";

ut_test(no_layout)
{
    char test_conf[1024];
    char error_msg[EXA_MAXSIZE_LINE + 1];
    int res;

    os_snprintf(test_conf, sizeof(test_conf), test_conf_bad_group_format, EXA_VERSION,
		"name=\"group\"", "", adm_hostname());

    res = adm_deserialize_from_memory(test_conf, strlen(test_conf),
                                      error_msg, false);
    UT_ASSERT_EQUAL(-ADMIND_ERR_CONFIG_LOAD, res);
    UT_ASSERT_EQUAL_STR("Unspecified layout type for group \"group\"", error_msg);
}

ut_test(bad_layout_name)
{
    char test_conf[1024];
    char error_msg[EXA_MAXSIZE_LINE + 1];
    int res;

    os_snprintf(test_conf, sizeof(test_conf), test_conf_bad_group_format, EXA_VERSION,
		"name=\"group\"", "layout=\"dummy layout name\"", adm_hostname());

    res = adm_deserialize_from_memory(test_conf, strlen(test_conf),
                                      error_msg, false);
    UT_ASSERT_EQUAL(-ADMIND_ERR_CONFIG_LOAD, res);
    UT_ASSERT_EQUAL_STR("Invalid layout \"dummy layout name\" for group \"group\"", error_msg);
}

ut_test(empty_group_name)
{
    char test_conf[1024];
    char error_msg[EXA_MAXSIZE_LINE + 1];
    int res;

    os_snprintf(test_conf, sizeof(test_conf), test_conf_bad_group_format, EXA_VERSION,
		"name=\"\"", "layout=\"rain1\"", adm_hostname());

    res = adm_deserialize_from_memory(test_conf, strlen(test_conf),
                                      error_msg, false);
    UT_ASSERT_EQUAL(-ADMIND_ERR_CONFIG_LOAD, res);
    UT_ASSERT_EQUAL_STR("Empty value for attribute 'name' in <diskgroup>", error_msg);
}

ut_test(no_group_name_tag)
{
    char test_conf[1024];
    char error_msg[EXA_MAXSIZE_LINE + 1];
    int res;

    os_snprintf(test_conf, sizeof(test_conf), test_conf_bad_group_format, EXA_VERSION,
		"", "layout=\"rain1\"", adm_hostname());

    res = adm_deserialize_from_memory(test_conf, strlen(test_conf),
                                      error_msg, false);
    UT_ASSERT_EQUAL(-ADMIND_ERR_CONFIG_LOAD, res);
    UT_ASSERT_EQUAL_STR("Missing attribute 'name' in <diskgroup>", error_msg);
}

ut_test(group_ok)
{
    char test_conf[1024];
    char error_msg[EXA_MAXSIZE_LINE + 1];
    int res;

    os_snprintf(test_conf, sizeof(test_conf), test_conf_bad_group_format, EXA_VERSION,
		"name=\"group\"", "layout=\"rain1\"", adm_hostname());

    res = adm_deserialize_from_memory(test_conf, strlen(test_conf),
                                      error_msg, false);
    if (res != EXA_SUCCESS)
        ut_printf("-->%s<--- error: %s", test_conf, error_msg);
    UT_ASSERT_EQUAL(EXA_SUCCESS, res);
}


