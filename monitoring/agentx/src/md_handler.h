/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_HANDLER_H__
#define __MD_HANDLER_H__

#include "monitoring/md_com/include/md_com.h"
#include "monitoring/common/include/md_types.h"


void md_send_msg(const md_com_msg_t *tx_msg);
void md_send_trap(const md_msg_agent_trap_t *trap);

void md_handler_thread(void *pstop);



#endif /* __MD_HANDLER_H__ */
