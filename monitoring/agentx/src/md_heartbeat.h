/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_HEARTBEAT_H__
#define __MD_HEARTBEAT_H__

#include <stdbool.h>


/** indicates whether or not monitoring daemon is alive,
 *  depending on reception of an alive message lately...
 */
bool md_is_alive(void);

/** callback called from md_handler when an alive message
 *  has finally arrived.
 */
void md_received_alive(void);

void md_heartbeat_loop(void);

void md_heartbeat_thread(void *pstop);


#endif /* __MD_HEARTBEAT_H__ */
