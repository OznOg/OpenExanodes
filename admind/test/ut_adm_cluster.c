/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unit_testing.h>

#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_cluster.h"

#include "common/include/exa_error.h"
#include "common/include/exa_config.h"

#include "os/include/os_stdio.h"

static void add_node_2_cluster(int id, const char* name)
{
    struct adm_node *node;
    node = adm_node_alloc();
    UT_ASSERT(node);
    node->id = id;
    os_snprintf(node->name, EXA_MAXSIZE_NODENAME, "%s", name);
    UT_ASSERT(EXA_SUCCESS == adm_cluster_insert_node(node));
}

ut_setup()
{
    adm_cluster_init();

    add_node_2_cluster(1, "node_1");
    add_node_2_cluster(2, "node_2");
    add_node_2_cluster(3, "node_3");
    add_node_2_cluster(4, "node_4");
}


ut_cleanup()
{
    adm_cluster_cleanup();
}

#ifdef WITH_FS
/**
 * This test ensures partly that GULM master nodes can not be deleted
 * by exa_clnodedel command
 */
ut_test(gulm_masters_setting)
{
    exa_nodeset_t param_set;

    adm_cluster_set_param(EXA_OPTION_FS_GULM_MASTERS, "node_2 node_3");
    adm_cluster_get_param_nodeset(&param_set, EXA_OPTION_FS_GULM_MASTERS);

    UT_ASSERT(exa_nodeset_contains(&param_set, 2));
    UT_ASSERT(exa_nodeset_contains(&param_set, 3));
    UT_ASSERT( ! exa_nodeset_contains(&param_set, 1));
    UT_ASSERT( ! exa_nodeset_contains(&param_set, 4));
}
#endif

ut_test(set_and_get_io_barriers)
{
    bool val;
    int err = adm_cluster_set_param("io_barriers", "TRUE");
    UT_ASSERT_EQUAL(0, err);

    val = adm_cluster_get_param_boolean("io_barriers");

    UT_ASSERT_EQUAL(true, val);

    err = adm_cluster_set_param("io_barriers", "FALSE");
    UT_ASSERT_EQUAL(0, err);

    val = adm_cluster_get_param_boolean("io_barriers");

    UT_ASSERT_EQUAL(false, val);
}

ut_test(set_and_get_multicast_port)
{
    int err;
    int port;

    err = adm_cluster_set_param("multicast_port", "12345");
    UT_ASSERT_EQUAL(0, err);

    port = adm_cluster_get_param_int("multicast_port");

    UT_ASSERT(port == 12345);

    err = adm_cluster_set_param_default("multicast_port", "54321");
    UT_ASSERT_EQUAL(0, err);

    err = adm_cluster_set_param_to_default("multicast_port");
    UT_ASSERT_EQUAL(0, err);

    port = adm_cluster_get_param_int("multicast_port");

    UT_ASSERT(port == 54321);

}

ut_test(adm_cluster_log_tuned_params)
{
    int err = adm_cluster_set_param_default("multicast_port", "54321");
    UT_ASSERT_EQUAL(0, err);

    adm_cluster_log_tuned_params();
}

ut_test(set_and_get_invalid_param)
{
    int err;

    err = adm_cluster_set_param("gniiiiiiiiiii", "12345");
    UT_ASSERT_EQUAL(-EXA_ERR_SERVICE_PARAM_UNKNOWN, err);
    err = adm_cluster_set_param_default("gniiiiiii", "54321");
    UT_ASSERT_EQUAL(-EXA_ERR_SERVICE_PARAM_UNKNOWN, err);
    err = adm_cluster_set_param_to_default("gniiiiiii");
    UT_ASSERT_EQUAL(-EXA_ERR_SERVICE_PARAM_UNKNOWN, err);

    err = adm_cluster_set_param("multicast_port", "this is not a number");
    UT_ASSERT_EQUAL(-EXA_ERR_INVALID_VALUE, err);

}

ut_test(adm_cluster_nb_nodes)
{
    UT_ASSERT_EQUAL(4, adm_cluster_nb_nodes());
}

ut_test(adm_cluster_leadership)
{
    adm_leader_set = false;
    UT_ASSERT_EQUAL(false, adm_is_leader());
    UT_ASSERT(NULL == adm_leader());
    UT_ASSERT(NULL == adm_myself());

    adm_leader_set = true;
    adm_leader_id = adm_my_id = 0;
    UT_ASSERT_EQUAL(true, adm_is_leader());
    UT_ASSERT(NULL == adm_myself());

    /* node 0 does not exist... */
    UT_ASSERT(NULL == adm_leader());

    adm_leader_id = 2;
    UT_ASSERT_EQUAL(false, adm_is_leader());

    /* node 0 does not exist... */
    UT_ASSERT_EQUAL_STR("node_2", adm_leader()->name);
}

ut_test(adm_cluster_first_node_at)
{
    UT_ASSERT_EQUAL_STR("node_1", adm_cluster_first_node_at(0)->name);
    UT_ASSERT(NULL == adm_cluster_first_node_at(5));
}

ut_test(adm_cluster_get_node_by_id)
{
    UT_ASSERT_EQUAL_STR("node_3", adm_cluster_get_node_by_id(3)->name);
}

ut_test(add_node_to_cluster)
{
    struct adm_node *node;
    node = adm_node_alloc();
    UT_ASSERT(node);
    node->id = 3;
    os_snprintf(node->name, EXA_MAXSIZE_NODENAME, "%s", "node_7");
    UT_ASSERT(-EEXIST == adm_cluster_insert_node(node));

    node->id = 15;
    os_snprintf(node->name, EXA_MAXSIZE_NODENAME, "%s", "node_3");
    UT_ASSERT(-EEXIST == adm_cluster_insert_node(node));
}

ut_test(volumes)
{
    exa_uuid_t uuid;
    UT_ASSERT(NULL == adm_cluster_get_volume_by_name("not a group", "too bad !"));
/*  FIXME find how to test this, for now it asserts...
    UT_ASSERT(NULL == adm_cluster_get_volume_by_name(NULL, "too bad !"));
    UT_ASSERT(NULL == adm_cluster_get_volume_by_name(NULL, NULL));
    */
    UT_ASSERT(NULL == adm_cluster_get_volume_by_uuid(&uuid));
    UT_ASSERT(NULL == adm_cluster_get_volume_by_uuid(NULL));
}
