/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_MESSAGING_H__
#define __MD_MESSAGING_H__

#include "monitoring/common/include/md_types.h"
#include <stdbool.h>


bool md_messaging_setup();
void md_messaging_cleanup();

bool md_messaging_recv_control(md_msg_control_t *control);
bool md_messaging_ack_control(int error);
bool md_messaging_reply_control_status(md_service_status_t status);

bool md_messaging_recv_event(md_msg_event_t *event);

#endif /* __MD_MESSAGING_H__ */
