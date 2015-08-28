/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md_com/include/md_com_msg.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


md_com_msg_t* md_com_msg_alloc_tx(int msg_type, const char* payload, int payload_size)
{
    md_com_msg_t *msg = (md_com_msg_t *)malloc(sizeof(md_com_msg_t));
    assert(msg != NULL);
    msg->size = payload_size;
    msg->type = msg_type;
    msg->payload = malloc(payload_size);
    assert(msg->payload != NULL);
    memcpy(msg->payload, payload, payload_size);
    return msg;
}

#include <stdio.h>


md_com_msg_t* md_com_msg_alloc_rx()
{
    md_com_msg_t *msg = (md_com_msg_t *)malloc(sizeof(md_com_msg_t));
    assert(msg != NULL);
    memset(msg, 0, sizeof(md_com_msg_t));
    return msg;
}


void md_com_msg_free_message(md_com_msg_t* msg)
{
    if (msg->payload != NULL)
	free(msg->payload);
    free(msg);
    msg = NULL;
}



