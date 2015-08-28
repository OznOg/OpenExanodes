/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_SRV_COM_H__
#define __MD_SRV_COM_H__

#include "monitoring/md_com/include/md_com.h"
#include <stdbool.h>

void md_srv_com_thread(void *dummy);

void md_srv_com_thread_stop(void);

void md_srv_com_static_init(void);
void md_srv_com_static_clean(void);
bool md_srv_com_is_agentx_alive(void);
void md_srv_com_send_msg(const md_com_msg_t* tx_msg);

#endif /* __MD_SRV_COM_H__ */
