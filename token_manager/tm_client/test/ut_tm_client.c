/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "token_manager/tm_client/include/tm_client.h"
#include "token_manager/tm_server/src/token_manager.h" /* for token_manager_data_t */
#include "token_manager/tm_server/include/tm_server.h" /* for TM_TOKENS_MAX */
#include "token_manager/tm_server/include/token_msg.h"

#include "os/include/os_process.h"
#include "os/include/os_random.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#define NUM_TEST_CLUSTERS  TM_TOKENS_MAX

typedef struct
{
    exa_uuid_t uuid;
    token_manager_t *tms[2];
} test_cluster_t;

static test_cluster_t clusters[NUM_TEST_CLUSTERS];
static token_manager_t *user_tm;

#ifdef WIN32
static char *test_tokens_file = "C:\\tokens";
#else
static char *test_tokens_file = "/tmp/tokens";
#endif
static token_manager_data_t data;
static os_thread_t server_thread = 0;

static void __setup_cluster(test_cluster_t *c)
{
    os_get_random_bytes(&c->uuid, sizeof(exa_uuid_t));
    c->tms[0] = NULL;
    c->tms[1] = NULL;
}

ut_setup()
{
    int i;

    os_random_init();
    unlink(test_tokens_file);
    data.file = test_tokens_file;

    /* We generate two random ports as when two instances of this UT are run at
     * the same time, one can block the other as they try to open the same port.
     *
     * The operation done below with the modulo and the addition is to ensure
     * the value is bound between 1025 and 65535, which is the default
     * non-privileged user accessible ports range.
     */
    data.port = (os_process_id() % 64511) + 1025;
    data.priv_port = (data.port + 1) % 64511 + 1025;

    UT_ASSERT(data.port > 1024);
    UT_ASSERT(data.priv_port > 1024);
    UT_ASSERT(data.port != data.priv_port);

#ifdef WIN32
    data.logfile = "nul";
#else
    data.logfile = "/dev/null";
#endif

    os_thread_create(&server_thread, 0, token_manager_thread, &data);
    os_sleep(1);

    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
        __setup_cluster(&clusters[i]);

    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
    {
        UT_ASSERT_EQUAL(0, tm_init(&clusters[i].tms[0], "127.0.0.1", data.port));
        UT_ASSERT_EQUAL(0, tm_connect(clusters[i].tms[0]));
        UT_ASSERT_EQUAL(0, tm_init(&clusters[i].tms[1], "127.0.0.1", data.port));
        UT_ASSERT_EQUAL(0, tm_connect(clusters[i].tms[1]));
    }

    UT_ASSERT(tm_init(&user_tm, "127.0.0.1", data.priv_port) == 0);
    UT_ASSERT_EQUAL(0, tm_connect(user_tm));
}

ut_cleanup()
{
    int i;

    tm_disconnect(user_tm);
    tm_free(&user_tm);

    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
    {
        tm_disconnect(clusters[i].tms[0]);
        tm_free(&clusters[i].tms[0]);

        tm_disconnect(clusters[i].tms[1]);
        tm_free(&clusters[i].tms[1]);
    }

    os_sleep(1);
    token_manager_thread_stop();
    os_thread_join(server_thread);
    unlink(test_tokens_file);

    os_random_cleanup();
}

#define __REQUEST(cluster, node_id) \
    tm_request_token((cluster)->tms[(node_id)], &(cluster)->uuid, (node_id))

#define __RELEASE(cluster, node_id) \
    tm_release_token((cluster)->tms[(node_id)], &(cluster)->uuid, (node_id))

#define __FORCE_RELEASE(cluster) \
    tm_force_token_release(user_tm, &(cluster)->uuid)

ut_test(tm_check_connection_succeeds_when_connected)
{
    UT_ASSERT_EQUAL(0, tm_check_connection(clusters[0].tms[0],
                                           &clusters[0].uuid, 0));
}

ut_test(tm_check_connection_fails_when_disconnected)
{
    token_manager_thread_stop();
    os_thread_join(server_thread);

    UT_ASSERT_EQUAL(-EBADF, tm_check_connection(clusters[0].tms[0],
                                                &clusters[0].uuid, 0));
}

ut_test(connecting_to_NULL_token_manager_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, tm_init(NULL, "127.0.0.1", 0));
}

ut_test(connecting_to_NULL_address_returns_EINVAL)
{
    token_manager_t *tm;

    UT_ASSERT_EQUAL(-EINVAL, tm_init(&tm, NULL, 0));
}

ut_test(connecting_to_empty_address_returns_EINVAL)
{
    token_manager_t *tm;

    UT_ASSERT_EQUAL(-EINVAL, tm_init(&tm, "", 0));
}

ut_test(connecting_to_ill_formed_address_returns_EINVAL)
{
    token_manager_t *tm;

    UT_ASSERT_EQUAL(-EINVAL, tm_init(&tm, "hello", 0));
    UT_ASSERT_EQUAL(-EINVAL, tm_init(&tm, "  127.0.0.1", 0));
}

ut_test(requesting_token_from_NULL_token_manager_returns_EINVAL)
{
    exa_uuid_t uuid;

    os_get_random_bytes(&uuid, sizeof(uuid));
    UT_ASSERT_EQUAL(-EINVAL, tm_request_token(NULL, &uuid, 5));
}

ut_test(requesting_token_with_NULL_uuid_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, tm_request_token(user_tm, NULL, 5));
}

ut_test(requesting_token_with_invalid_node_id_returns_EINVAL)
{
    exa_uuid_t uuid;

    os_get_random_bytes(&uuid, sizeof(uuid));
    UT_ASSERT_EQUAL(-EINVAL, tm_request_token(user_tm, &uuid, EXA_MAX_NODES_NUMBER));
    UT_ASSERT_EQUAL(-EINVAL, tm_request_token(user_tm, &uuid, EXA_NODEID_LOCALHOST));
    UT_ASSERT_EQUAL(-EINVAL, tm_request_token(user_tm, &uuid, EXA_NODEID_NONE));
}

ut_test(token_holder_can_acquire_token_again)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
}

ut_test(only_one_node_may_hold_the_token)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT(__REQUEST(&clusters[0], 1) < 0);
}

ut_test(non_holder_cant_release_token)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT(__RELEASE(&clusters[0], 1) < 0);
}

ut_test(holder_can_release_token)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT_EQUAL(0, __RELEASE(&clusters[0], 0));
}

ut_test(releasing_non_held_token)
{
    UT_ASSERT_EQUAL(-ENOENT, __RELEASE(&clusters[0], 0));
}

ut_test(acquire_release_then_reacquire_by_same_node)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT_EQUAL(0, __RELEASE(&clusters[0], 0));

    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
}

ut_test(acquire_release_then_reacquire_by_other_node)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT_EQUAL(0, __RELEASE(&clusters[0], 0));

    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 1));
}

ut_test(force_release_of_non_existent_token)
{
    UT_ASSERT_EQUAL(0, __FORCE_RELEASE(&clusters[0]));
}

ut_test(force_release_of_token_held_by_some_node)
{
    UT_ASSERT_EQUAL(0, __REQUEST(&clusters[0], 0));
    UT_ASSERT_EQUAL(0, __FORCE_RELEASE(&clusters[0]));
}

ut_test(in_all_clusters_nodes_alternatively_acquire_their_token)
{
    int i;

    /* All 0 nodes acquires their cluster token */
    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
        UT_ASSERT_EQUAL(0, __REQUEST(&clusters[i], 0));

    /* All 1 nodes try to acquire it too, but fail */
    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
        UT_ASSERT(__REQUEST(&clusters[i], 1) < 0);

    /* All 0 nodes release their cluster token */
    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
        UT_ASSERT_EQUAL(0, __RELEASE(&clusters[i], 0));

    /* All 1 nodes succeed in acquiring their cluster token */
    for (i = 0; i < NUM_TEST_CLUSTERS; i++)
        UT_ASSERT_EQUAL(0, __REQUEST(&clusters[i], 1));
}

ut_test(when_max_tokens_exceeded_client_is_rejected)
{
    test_cluster_t one_too_many;

    __setup_cluster(&one_too_many);
    UT_ASSERT_EQUAL(0, tm_init(&one_too_many.tms[0], "127.0.0.1",
                                        data.port));
    UT_ASSERT_EQUAL(-ENOENT, tm_connect(one_too_many.tms[0]));
}
