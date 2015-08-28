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
#include "admind/src/adm_hostname.h"

#include "common/include/exa_error.h"
#include "os/include/os_stdio.h"

static struct adm_node *node_1;
static struct adm_node *node_2;
static struct adm_node *node_3;
static struct adm_node *node_myself;
static const char *my_name;

ut_setup()
{
    adm_cluster_init();

    node_1 = adm_node_alloc();
    node_2 = adm_node_alloc();
    node_3 = adm_node_alloc();
    node_myself = adm_node_alloc();

    UT_ASSERT(node_1 && node_2 && node_3);

    my_name = adm_hostname();

    node_1->id = 1;
    os_snprintf(node_1->name, EXA_MAXSIZE_NODENAME, "%s", "node_1");
    UT_ASSERT(EXA_SUCCESS == adm_cluster_insert_node(node_1));

    node_2->id = 2;
    os_snprintf(node_2->name, EXA_MAXSIZE_NODENAME, "%s", "node_2");
    UT_ASSERT(EXA_SUCCESS == adm_cluster_insert_node(node_2));

    node_3->id = 3;
    os_snprintf(node_3->name, EXA_MAXSIZE_NODENAME, "%s", "node_3");
    UT_ASSERT(EXA_SUCCESS == adm_cluster_insert_node(node_3));

    /* FIXME : nodename and hostname SHOULD be 2 different things ! */
    node_myself->id = 4;
    os_snprintf(node_myself->name, EXA_MAXSIZE_NODENAME, "%s", my_name);
    os_snprintf(node_myself->hostname, EXA_MAXSIZE_NODENAME, "%s", my_name);
    UT_ASSERT_EQUAL(EXA_SUCCESS, adm_cluster_insert_node(node_myself));

}


ut_cleanup()
{
    adm_cluster_cleanup();
}


ut_test(nodeset_from_names)
{
    int res;
    exa_nodeset_t nodeset;

    res = adm_nodeset_from_names(&nodeset, "");
    UT_ASSERT_EQUAL(EXA_SUCCESS, res);

    res = adm_nodeset_from_names(&nodeset, "   ");
    UT_ASSERT_EQUAL(EXA_SUCCESS, res);

    res = adm_nodeset_from_names(&nodeset, "node_2");
    UT_ASSERT_EQUAL(EXA_SUCCESS, res);

    res = adm_nodeset_from_names(&nodeset, "node_3   node_1");
    UT_ASSERT_EQUAL(EXA_SUCCESS, res);

    res = adm_nodeset_from_names(&nodeset, "node_23");
    UT_ASSERT(EXA_SUCCESS != res);

    res = adm_nodeset_from_names(&nodeset, "node_1node_2");
    UT_ASSERT(EXA_SUCCESS != res);
}

ut_test(adm_nodeset_contains_me)
{
    /* Should already be properly covered with "exa_nodeset_contains" */
    exa_nodeset_t nodeset;
    char nodeset_string[(EXA_MAXSIZE_NODENAME+1)*3];

    adm_nodeset_from_names(&nodeset, "node_1 node_3");
    UT_ASSERT(!adm_nodeset_contains_me(&nodeset));

    os_snprintf(nodeset_string, sizeof(nodeset_string),
	     "node_1 node_3 %s", my_name);
    adm_nodeset_from_names(&nodeset, nodeset_string);
    UT_ASSERT(adm_nodeset_contains_me(&nodeset));
}

ut_test(adm_nodeid_to_name)
{
    bool res = strcmp(adm_nodeid_to_name(2), "node_2") == 0;
    UT_ASSERT(res);
}

ut_test(adm_nodeid_from_name)
{
    int res;
    res = adm_nodeid_from_name("node_2");
    UT_ASSERT_EQUAL(res, 2);

    res = adm_nodeid_from_name("");
    UT_ASSERT_EQUAL(res, 4);
}

ut_test(adm_nodeset_to_names)
{
    exa_nodeset_t nodeset;
    char list[(EXA_MAXSIZE_NODENAME+1)*3];

    /* initialize with string content */
    exa_nodeset_reset(&nodeset);
    exa_nodeset_add(&nodeset, 1);
    exa_nodeset_add(&nodeset, 3);
    memset(list, 0, sizeof(list));
    adm_nodeset_to_names(&nodeset, list, sizeof(list));
    UT_ASSERT(!strcmp(list, "node_1 node_3"));

    /* test small string behavior */
    memset(list, 0, sizeof(list));

    adm_nodeset_to_names(&nodeset, list, strlen("node_1 n"));
    /* TODO : Is it voluntary to insert a " " after the last name ? */
    UT_ASSERT(!strcmp(list, "node_1 "));

}

ut_test(adm_nodeset_set_all)
{
    exa_nodeset_t nodeset;
    exa_nodeid_t node;

    adm_nodeset_set_all(&nodeset);
    exa_nodeset_foreach(&nodeset, node)
    {
	UT_ASSERT (!((node > 4) || (node <1)));
    }
    UT_ASSERT(exa_nodeset_contains(&nodeset, 1));
    UT_ASSERT(exa_nodeset_contains(&nodeset, 2));
    UT_ASSERT(exa_nodeset_contains(&nodeset, 3));
    UT_ASSERT(exa_nodeset_contains(&nodeset, 4));
}
