/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_STATS_H__
#define __VRT_STATS_H__

#include "vrt/virtualiseur/include/vrt_request.h"
#include "vrt/virtualiseur/include/vrt_msg.h"

void vrt_stats_restart(struct vrt_stats_volume *stats);
void vrt_stats_handle_message(const struct vrt_stats_request *request, vrt_reply_t *reply);
void vrt_stat_request_begin(struct vrt_request *request);
void vrt_stat_request_done(struct vrt_request *request, bool failed);

#endif /* __VRT_STATS_H__ */
