/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_CMD_H__
#define __VRT_CMD_H__

#include "vrt/virtualiseur/include/vrt_msg.h"

void vrt_cmd_handle_message_init(void);
void vrt_cmd_handle_message_clean(void);
size_t vrt_cmd_handle_message(const vrt_cmd_t *msg, vrt_reply_t *reply);
int vrt_cmd_group_unfreeze(const struct VrtGroupUnfreeze *cmd);

#endif /* __VRT_CMD_H__ */
