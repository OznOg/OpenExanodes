/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_COM_H__
#define __MD_COM_H__


#include "monitoring/md_com/include/md_com_msg.h"



typedef enum {
    COM_SUCCESS = 0,
    COM_TOO_MANY_CONNECTIONS,
    COM_READ_ERROR,
    COM_WRITE_ERROR,
    COM_CONNECTION_CLOSED,
    COM_UNKNOWN_ERROR
} md_com_error_code_t;





/**
   Create a new server connection.

   @param arg Info needed for connection initiation.
   @param connection_id Out parameter where the created connection id is stored
   @return Error code.

*/
md_com_error_code_t md_com_listen(const void *arg, int* connection_id);


/**
   Wait for an incoming connection.
   This function is blocking.

   @param connection_id connection identifier
   @return remote connection identifier

*/
md_com_error_code_t md_com_accept(int server_connection_id, int* client_connection_id);


/**
   Receives a message.

   @param connection_id Connection identifier
   @param rx_msg Message to receive.
   @return Error code.

*/
md_com_error_code_t md_com_recv_msg(int connection_id, md_com_msg_t* rx_msg);


/**
   Inititates a new client connection.

   @param arg Implementation dependant arg needed for connect.
   @param connection_id Out parameter for created client connection id.
   @return Error code.

*/
md_com_error_code_t md_com_connect(const void *arg, int* connection_id);


/**
   Sends a message.

   @param connection_id Connection identifier
   @param tx_msg Message to send.
   @return Error code.

*/
md_com_error_code_t md_com_send_msg(int connection_id, const md_com_msg_t* tx_msg);


/**
   Closes a connection.

   @param connection_id Client or server connection id.
   @return Error code.

*/
md_com_error_code_t md_com_close(int connection_id);


#endif /* __MD_COM_H__ */
