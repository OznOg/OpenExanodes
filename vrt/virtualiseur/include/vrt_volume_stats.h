/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef VRT_VOLUME_STATS_H
#define VRT_VOLUME_STATS_H

#include "vrt/virtualiseur/include/vrt_common.h"
#include "os/include/os_inttypes.h"

struct vrt_stats_begin
{
    uint64_t nb_sect_read;
    uint64_t nb_sect_write;
    uint64_t nb_req_read;
    uint64_t nb_req_write;
    uint64_t nb_seeks_read;
    uint64_t nb_seeks_write;
    uint64_t nb_seek_dist_read;
    uint64_t nb_seek_dist_write;
};

struct vrt_stats_done
{
    uint64_t nb_sect_read;
    uint64_t nb_sect_write;
    uint64_t nb_req_read;
    uint64_t nb_req_write;
    uint64_t nb_req_err;
};

struct vrt_stats_volume
{
    /* FIXME: Missing locking here. */

    uint64_t last_reset;

    struct {
	struct vrt_stats_begin info;
	vrt_io_type_t prev_request_type;
	uint64_t next_sector;
    } begin;
    struct {
	struct vrt_stats_done info;
    } done;
};

#endif /* VRT_VOLUME_STATS_H */
