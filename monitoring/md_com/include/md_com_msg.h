/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_COM_MSG_H__
#define __MD_COM_MSG_H__



typedef struct {
    int    size;    //!< size of message excluding msg meta info (size and type)
    int    type;    //!< type of message
    char   *payload;
} md_com_msg_t;



/**
   Creates a new msg for sending purpose.
   Payload is memcopied into message payload member.

   @param msg_type Message type.
   @param payload Payload to be embedded in message.
   @param payload_size Payload size.
   @return The created message or NULL if it cannot be allocated.

*/
md_com_msg_t* md_com_msg_alloc_tx(int msg_type, const char* payload, int payload_size);


/**
   Creates a new msg for receiving purpose.

   @return The created message or NULL if it cannot be allocated.

*/
md_com_msg_t* md_com_msg_alloc_rx();


/**
   Frees message memory space.

   @param msg Message to unalloc. Set to NULL once done.
   @param payload

*/
void md_com_msg_free_message(md_com_msg_t* msg);




#endif /* __MD_COM_MSG_H__ */
