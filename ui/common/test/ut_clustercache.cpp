/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "unit_testing.h"

#include "ui/common/include/clustercache.h"

#include <stdlib.h>

#include <stdexcept>
#include <iostream>
#include <string>

#include "os/include/os_stdio.h"
#include "os/include/os_dir.h"

#ifdef WIN32
#define putenv _putenv
#endif

using std::exception;

static exa_uuid_t uuid;

/* Must be global: this variable will be part of the environment after
 * putenv().
 */
static char cache_env[512];

static void __set_uuid(void)
{
    UT_ASSERT(uuid_scan("12345678:12345678:12345678:12345678", &uuid) == 0);
}

/** Expected result of test performed by helper function __read_cache() */
typedef enum { EXPECT_FAIL, EXPECT_SUCCESS } expected_t;

/**
 * Helper function.
 *
 * Attempts to build a cluster cache for the given cluster name.
 *
 * @param[in] cluster_name        Name of the cluster
 * @param[in] expected            Expected outcome of the test
 * @param[in] expected_error_msg  Expected error message ("" if doesn't matter)
 */
static void __read_cache(const std::string cluster_name, expected_t expected,
                         const std::string expected_error_msg = "")
{
    os_snprintf(cache_env, sizeof(cache_env),
                "EXANODES_CACHE_DIR=%s/ut_caches/", getenv("srcdir"));
    putenv(cache_env);

    try
    {
        ClusterCache cc(cluster_name);
    }
    catch (exception &exc)
    {
        std::string msg(exc.what());
        if (expected == EXPECT_FAIL)
        {
            if (expected_error_msg.empty() || msg.find(expected_error_msg) == 0)
                return;
        }
    }

    if (expected == EXPECT_FAIL)
        UT_FAIL();
}

UT_SECTION(cache_name)

ut_setup()
{
    __set_uuid();
}

ut_cleanup()
{
}

ut_test(invalid_cache_name)
{
    __read_cache("", EXPECT_FAIL);
}

ut_test(cache_name_and_uuid)
{
    std::string cluster_name("dummy");
    ClusterCache cc(cluster_name, uuid, "");

    UT_ASSERT(uuid_is_equal(&cc.uuid, &uuid)
              && cc.name == cluster_name);
}

UT_SECTION(node_addition)

ut_setup()
{
    __set_uuid();
}

ut_cleanup()
{
}

ut_test(get_unknown_node)
{
    ClusterCache cc("dummy", uuid, "");
    UT_ASSERT(cc.to_nodename("toto") == "");
}

ut_test(add_empty_nodename)
{
    try
    {
        ClusterCache cc("dummy", uuid, "");
        cc.add_node("", "hello.world");
    }
    catch (exception &exc)
    {
        std::string msg(exc.what());
        if (msg == "invalid nodename")
            return;
    }

    UT_FAIL();
}

ut_test(add_empty_hostname)
{
    try
    {
        ClusterCache cc("dummy", uuid, "");
        cc.add_node("hello", "");
    }
    catch (exception &exc)
    {
        std::string msg(exc.what());
        if (msg == "invalid hostname")
            return;
    }

    UT_FAIL();
}

ut_test(add_node)
{
    ClusterCache cc("dummy", uuid, "");

    cc.add_node("node1", "lemonde.fr");
    cc.add_node("node2", "liberation.fr");

    UT_ASSERT(cc.to_nodename("lemonde.fr") == "node1"
              && cc.to_nodename("liberation.fr") == "node2");
}

ut_test(add_duplicate_node)
{
    ClusterCache cc("dummy", uuid, "");

    cc.add_node("node1", "lwn.net");
    cc.add_node("node1", "kerneltrap.org");

    UT_ASSERT(cc.to_nodename("kerneltrap.org") == "node1");
}

UT_SECTION(node_removal)

ut_setup()
{
    __set_uuid();
}

ut_cleanup()
{
}

ut_test(del_unknown_node)
{
    ClusterCache cc("dummy", uuid, "");
    cc.del_node("titi");
}

ut_test(del_node)
{
    ClusterCache cc("dummy", uuid, "");

    cc.add_node("toto", "bonjour");
    cc.del_node("toto");

    UT_ASSERT(cc.to_nodename("toto") == "");
}

UT_SECTION(node_map)

ut_setup()
{
    __set_uuid();
}

ut_cleanup()
{
}

ut_test(get_empty_map)
{
    ClusterCache cc("dummy", uuid, "");
    std::map<std::string, std::string> map = cc.get_node_map();

    UT_ASSERT(map.size() == 0);
}

ut_test(get_map)
{
    ClusterCache cc("dummy", uuid, "");

    cc.add_node("one", "dummy.stuff");
    cc.add_node("two", "silly.thing");
    cc.add_node("three", "one.too.many");

    std::map<std::string, std::string> map = cc.get_node_map();
    UT_ASSERT(map.size() == 3 && map["dummy.stuff"] == "one"
              && map["silly.thing"] == "two" && map["one.too.many"] == "three");
}

UT_SECTION(read_cache_file)

ut_setup()
{
    __set_uuid();
}

ut_cleanup()
{
}

ut_test(read_nonexistent_cache_file)
{
    __read_cache("grumpf", EXPECT_FAIL, "could not open cache file");
}

ut_test(read_empty_cache_file)
{
    __read_cache("empty", EXPECT_FAIL,
                 "unexpected end of file reading cache file");
}

ut_test(read_cache_file_with_invalid_uuid)
{
    __read_cache("invalid_uuid", EXPECT_FAIL,
                 "failed to parse UUID from cache file");
}

ut_test(read_cache_file_with_no_nodes)
{
    __read_cache("no_nodes", EXPECT_FAIL, "no nodes in cache file");
}

ut_test(read_cache_file_with_invalid_node)
{
    __read_cache("invalid_node", EXPECT_FAIL, "invalid node line:");
}

ut_test(read_cache_file_with_blank_lines)
{
    __read_cache("blank_lines", EXPECT_SUCCESS);
}

ut_test(read_cache_file_with_many_spaces)
{
    __read_cache("many_spaces", EXPECT_SUCCESS);
}

ut_test(read_cache_file_three_nodes)
{
    __read_cache("3_nodes", EXPECT_SUCCESS);
}

UT_SECTION(save_cache_file)

/**
 * Get a temporary path for the cluster cache
 *
 * @param[out] path  Temporary cluster cache path
 * @param[in]  size  Size of the path variable
 */
static void __clustercache_get_tmp_path(char *path, size_t size)
{
#ifdef WIN32
    os_snprintf(path, size, "%s\\ut_clustercache", getenv("TMP"));
#else
    os_snprintf(path, size, "/tmp/ut_clustercache");
#endif
}

ut_setup()
{
    char path[128];

    __set_uuid();

    __clustercache_get_tmp_path(path, sizeof(path));
    os_dir_remove_tree(path);
    os_dir_create(path);

    os_snprintf(cache_env, sizeof(cache_env),
                "EXANODES_CACHE_DIR=%s", path);
    putenv(cache_env);
}

ut_cleanup()
{
    char path[128];

    __clustercache_get_tmp_path(path, sizeof(path));
    os_dir_remove_tree(path);
}

ut_test(save_empty_cache_file)
{
    ClusterCache cc("dummy", uuid, "");
    cc.save();
}

ut_test(save_existing_cache_file)
{
    ClusterCache cc("dummy", uuid, "");

    cc.add_node("one", "premier.noeud");
    cc.add_node("two", "second.noeud");

    /* Save two times: first to create the file and then again over the
     * existing file */
    cc.save();
    cc.save();
}

ut_test(save_empty_existing_cache_file_different_uuid)
{
    try
    {
        ClusterCache cc("one", uuid, "");
        cc.save();

        exa_uuid_t uuid2;
        UT_ASSERT(uuid_scan("ABCDEF12:ABCDEF12:ABCDEF12:ABCDEF12", &uuid2) == 0);
        ClusterCache cc2("one", uuid2, "");
        cc2.save();
    }
    catch (...)
    {
        UT_FAIL();
    }

    /* XXX I'd say that overwriting a cache file with a different uuid
     * shouldn't be allowed even if there aren't any nodes in the cache! */
}

ut_test(save_nonempty_existing_cache_file_different_uuid)
{
    try
    {
        ClusterCache cc1("one", uuid, "");
        cc1.add_node("one", "premier.noeud");
        cc1.add_node("two", "second.noeud");
        cc1.save();

        exa_uuid_t uuid2;
        UT_ASSERT(uuid_scan("ABCDEF12:ABCDEF12:ABCDEF12:ABCDEF12", &uuid2) == 0);
        ClusterCache cc2("one", uuid2, "");
        cc2.save();
    }
    catch (...)
    {
        return;
    }

    UT_FAIL();
}

ut_test(save_then_read_cache_file)
{
    std::map<std::string, std::string> node_defs;
    node_defs["sam10.toulouse"] = "noeud1";
    node_defs["sam35.toulouse"] = "noeud2";
    node_defs["sam79.toulouse"] = "noeud3";

    ClusterCache cc("saveread", uuid, "");
    std::map<std::string, std::string>::const_iterator it;
    for (it = node_defs.begin(); it != node_defs.end(); it++)
        cc.add_node(it->second, it->first);

    cc.save();

    ClusterCache cc2("saveread");
    UT_ASSERT(uuid_is_equal(&cc2.uuid, &cc.uuid)
              && cc2.name == cc.name
              && cc2.get_node_map() == node_defs);
}
