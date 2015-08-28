/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md_com/include/md_com.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <unit_testing.h>
#include <unistd.h>


typedef enum {
    TEST_PAYLOAD1,
    TEST_PAYLOAD2,
    TEST_PAYLOAD3,
    TEST_PAYLOAD4
} md_msg_test_payload_type_t;


typedef struct {
    int val1;
    int val2;
} md_msg_test_payload1_t;


typedef struct {
    double val1;
    char val2;
} __attribute__((packed)) md_msg_test_payload2_t;

typedef struct {
    int values[65536];
} md_msg_test_payload3_t;

/* empty payload */
typedef struct {
} md_msg_test_payload4_t;




void test_message_exchange_thread(void *unused)
{
    int i, ret;
    int server_connection_id, client_connection_id;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg, *tx_msg;
    md_msg_test_payload1_t *payload1;
    md_msg_test_payload2_t payload2 = { .val1 = -78.23 , .val2 = 'Z' };

    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    for (i = 0; i < 100; ++i)
    {
	rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
	UT_ASSERT(rx_msg != NULL);

	ret = md_com_recv_msg(client_connection_id, rx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	payload1 = (md_msg_test_payload1_t*) rx_msg->payload;
	UT_ASSERT_EQUAL(23, payload1->val1);
	UT_ASSERT_EQUAL(25, payload1->val2);

	md_com_msg_free_message(rx_msg);

	tx_msg = (md_com_msg_t *)md_com_msg_alloc_tx(TEST_PAYLOAD2,
						     (const char*) &payload2,
						     sizeof(payload2));

	UT_ASSERT(tx_msg != NULL);

	ret = md_com_send_msg(client_connection_id, tx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	md_com_msg_free_message(tx_msg);
    }
    md_com_close(server_connection_id);
}


ut_test(test_message_exchange)
{
    int client_connection_id;
    int i, ret;
    os_thread_t server_thread;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg, *tx_msg;
    md_msg_test_payload1_t payload1 = { .val1 = 23 , .val2 = 25 };
    md_msg_test_payload2_t *payload2;

    os_thread_create(&server_thread, 0,
                     test_message_exchange_thread, NULL);

    /* let server thread start and listen... */
    os_sleep(2);

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    for (i = 0; i < 100; ++i)
    {
	tx_msg = md_com_msg_alloc_tx(TEST_PAYLOAD1,
				     (const char*) &payload1,
				     sizeof(payload1));
	UT_ASSERT(tx_msg != NULL);

	ret = md_com_send_msg(client_connection_id, tx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	md_com_msg_free_message(tx_msg);

	rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
	UT_ASSERT(rx_msg != NULL);

	ret = md_com_recv_msg(client_connection_id, rx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	payload2 = (md_msg_test_payload2_t*) rx_msg->payload;
	UT_ASSERT_EQUAL(-78.23, payload2->val1);
	UT_ASSERT(payload2->val2 == 'Z');

	md_com_msg_free_message(rx_msg);
    }
    md_com_close(client_connection_id);

    os_thread_join(server_thread);

}




void test_empty_message_exchange_thread(void *unused)
{
    int i, ret;
    int server_connection_id, client_connection_id;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg, *tx_msg;
    md_msg_test_payload4_t payload2;

    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    for (i = 0; i < 100; ++i)
    {
	rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
	UT_ASSERT(rx_msg != NULL);

	ret = md_com_recv_msg(client_connection_id, rx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	UT_ASSERT_EQUAL(TEST_PAYLOAD4, rx_msg->type);

	md_com_msg_free_message(rx_msg);

	tx_msg = (md_com_msg_t *)md_com_msg_alloc_tx(TEST_PAYLOAD4,
						     (const char*) &payload2,
						     sizeof(payload2));

	UT_ASSERT(tx_msg != NULL);

	ret = md_com_send_msg(client_connection_id, tx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	md_com_msg_free_message(tx_msg);
    }
    md_com_close(server_connection_id);
}


ut_test(test_empty_message_exchange)
{
    int client_connection_id;
    int i, ret;
    os_thread_t server_thread;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg, *tx_msg;
    md_msg_test_payload4_t payload1;

    os_thread_create(&server_thread, 0,
                     test_empty_message_exchange_thread, NULL);

    /* let server thread start and listen... */
    os_sleep(2);

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    for (i = 0; i < 100; ++i)
    {
	tx_msg = md_com_msg_alloc_tx(TEST_PAYLOAD4,
				     (const char*) &payload1,
				     sizeof(payload1));
	UT_ASSERT(tx_msg != NULL);

	ret = md_com_send_msg(client_connection_id, tx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	md_com_msg_free_message(tx_msg);

	rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
	UT_ASSERT(rx_msg != NULL);

	ret = md_com_recv_msg(client_connection_id, rx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	UT_ASSERT_EQUAL(TEST_PAYLOAD4, rx_msg->type);

	md_com_msg_free_message(rx_msg);
    }
    md_com_close(client_connection_id);

    os_thread_join(server_thread);

}




void test_message_parallel_senders_sender_thread(void* arg)
{
    int i, ret;
    md_com_msg_t *tx_msg;
    md_msg_test_payload3_t payload3;

    /* client connection is shared by all senders */
    int client_connection_id = *((int *)arg);

    for (i=0; i<65536; ++i)
	payload3.values[i] = i;

    /* each sender sends 10000 messages */
    for (i = 0; i < 10000 ; ++i)
    {
	tx_msg = (md_com_msg_t *)md_com_msg_alloc_tx(TEST_PAYLOAD3,
						     (const char*) &payload3,
						     sizeof(payload3));

	UT_ASSERT(tx_msg != NULL);

	ret = md_com_send_msg(client_connection_id, tx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	md_com_msg_free_message(tx_msg);

    }
}



void test_message_parallel_senders_connect_thread(void *unused)
{
    int i, ret;
    int client_connection_id;
    const char* socket_path = "/tmp/ut_socket";
    os_thread_t sender_threads[10];

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    for (i = 0; i < 10; ++i)
    {
	os_thread_create(&sender_threads[i], 0,
                         test_message_parallel_senders_sender_thread,
                         &client_connection_id);
    }

    for (i = 0; i < 10; ++i)
	os_thread_join(sender_threads[i]);

    md_com_close(client_connection_id);
}


ut_test(test_heavy_message_parallel_senders) __ut_lengthy
{
    int server_connection_id, client_connection_id;
    int i, ret;
    os_thread_t connect_thread;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg;
    md_msg_test_payload3_t *payload3;
    md_msg_test_payload3_t expected_payload3;

    for (i=0; i<65536; ++i)
	expected_payload3.values[i] = i;

    /* receiver loop */
    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    os_thread_create(&connect_thread, 0,
                     test_message_parallel_senders_connect_thread,
                     NULL);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    for (i = 0; i < 100000; ++i)
    {
	rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
	UT_ASSERT(rx_msg != NULL);

	ret = md_com_recv_msg(client_connection_id, rx_msg);
	UT_ASSERT_EQUAL(COM_SUCCESS, ret);

	payload3 = (md_msg_test_payload3_t*) rx_msg->payload;

	UT_ASSERT(memcmp(&expected_payload3, payload3, rx_msg->size) == 0);

	md_com_msg_free_message(rx_msg);
    }
    md_com_close(server_connection_id);

    os_thread_join(connect_thread);

}





void test_sudden_client_connection_close_then_read(void *unused)
{
    int ret;
    int server_connection_id, client_connection_id;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg;

    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
    UT_ASSERT(rx_msg != NULL);

    os_sleep(1);

    ret = md_com_recv_msg(client_connection_id, rx_msg);
    UT_ASSERT_EQUAL(COM_CONNECTION_CLOSED, ret);

    md_com_msg_free_message(rx_msg);

    ret = md_com_close(server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);
}



ut_test(test_sudden_client_connection_close_then_read)
{
    int client_connection_id;
    int ret;
    os_thread_t server_thread;
    const char* socket_path = "/tmp/ut_socket";

    os_thread_create(&server_thread, 0,
                     test_sudden_client_connection_close_then_read, NULL);

    /* let server thread start and listen... */
    os_sleep(1);

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    /* close immediately to check error on server side */
    md_com_close(client_connection_id);

    os_thread_join(server_thread);

}



void test_sudden_client_connection_close_then_write(void *unused)
{
    int ret;
    int server_connection_id, client_connection_id;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *tx_msg;
    md_msg_test_payload1_t payload1 = { .val1 = 23 , .val2 = 25 };


    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    tx_msg = md_com_msg_alloc_tx(TEST_PAYLOAD1,
				 (const char*) &payload1,
				 sizeof(payload1));
    UT_ASSERT(tx_msg != NULL);

    os_sleep(1);

    ret = md_com_send_msg(client_connection_id, tx_msg);
    UT_ASSERT_EQUAL(COM_CONNECTION_CLOSED, ret);

    md_com_msg_free_message(tx_msg);

    ret = md_com_close(server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);
}




ut_test(test_sudden_client_connection_close_then_write)
{
    int client_connection_id;
    int ret;
    os_thread_t server_thread;
    const char* socket_path = "/tmp/ut_socket";

    os_thread_create(&server_thread, 0,
                     test_sudden_client_connection_close_then_write, NULL);

    /* let server thread start and listen... */
    os_sleep(1);

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    /* close immediately to check error on server side */
    md_com_close(client_connection_id);

    os_thread_join(server_thread);

}




void test_sudden_server_connection_close_then_read(void *unused)
{
    int ret;
    int server_connection_id, client_connection_id;
    const char* socket_path = "/tmp/ut_socket";

    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    /* close immediately to check error on client side */
    ret = md_com_close(client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);
}




ut_test(test_sudden_server_connection_close_then_read)
{
    int client_connection_id;
    int ret;
    os_thread_t server_thread;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *rx_msg;

    os_thread_create(&server_thread, 0,
                     test_sudden_server_connection_close_then_read, NULL);

    /* let server thread start and listen... */
    os_sleep(1);

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    rx_msg = (md_com_msg_t *)md_com_msg_alloc_rx();
    UT_ASSERT(rx_msg != NULL);

    os_sleep(1);

    ret = md_com_recv_msg(client_connection_id, rx_msg);
    UT_ASSERT_EQUAL(COM_CONNECTION_CLOSED, ret);

    md_com_msg_free_message(rx_msg);

    md_com_close(client_connection_id);

    os_thread_join(server_thread);

}



void test_sudden_server_connection_close_then_write(void *unused)
{
    int ret;
    int server_connection_id, client_connection_id;
    const char* socket_path = "/tmp/ut_socket";

    ret = md_com_listen(socket_path, &server_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    ret = md_com_accept(server_connection_id, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    /* close immediately to check error on client side */
    ret = md_com_close(client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);
}




ut_test(test_sudden_server_connection_close_then_write)
{
    int client_connection_id;
    int ret;
    os_thread_t server_thread;
    const char* socket_path = "/tmp/ut_socket";
    md_com_msg_t *tx_msg;

    md_msg_test_payload1_t payload1 = { .val1 = 23 , .val2 = 25 };

    os_thread_create(&server_thread, 0,
                     test_sudden_server_connection_close_then_write, NULL);

    /* let server thread start and listen... */
    os_sleep(1);

    ret = md_com_connect(socket_path, &client_connection_id);
    UT_ASSERT_EQUAL(COM_SUCCESS, ret);

    tx_msg = md_com_msg_alloc_tx(TEST_PAYLOAD1,
				 (const char*) &payload1,
				 sizeof(payload1));
    UT_ASSERT(tx_msg != NULL);

    os_sleep(1);

    ret = md_com_send_msg(client_connection_id, tx_msg);
    UT_ASSERT_EQUAL(COM_CONNECTION_CLOSED, ret);

    md_com_msg_free_message(tx_msg);

    md_com_close(client_connection_id);

    os_thread_join(server_thread);

}

