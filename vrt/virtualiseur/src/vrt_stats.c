/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_msg.h"
#include "vrt/virtualiseur/include/vrt_request.h"
#include "vrt/virtualiseur/include/vrt_volume.h"

#include "os/include/os_time.h"
#include "common/include/exa_error.h"
#include "common/include/threadonize.h"

#include "vrt/virtualiseur/src/vrt_module.h"

void vrt_stats_handle_message(const struct vrt_stats_request *request,
                              vrt_reply_t *reply)
{
    struct vrt_group *group = vrt_get_group_from_uuid(&request->group_uuid);

    memset(&reply->stats, 0, sizeof(reply->stats));

    if (group != NULL)
    {
	int i;
	struct vrt_volume *volume = NULL;

	for (i = 0; volume == NULL && i < NBMAX_VOLUMES_PER_GROUP; ++i)
	{
	    if (group->volumes[i] == NULL)
		continue;

	    if (strcmp(group->volumes[i]->name, request->volume_name) == 0)
		volume = group->volumes[i];
	}

	if (volume != NULL)
	{
	    reply->stats.last_reset = volume->stats.last_reset;
	    if (volume->stats.last_reset == 0)
		reply->stats.now = 0;
	    else
		reply->stats.now = os_gettimeofday_msec();
	    reply->stats.begin = volume->stats.begin.info;
	    reply->stats.done = volume->stats.done.info;

	    if (request->reset)
		vrt_volume_reset_stats(volume);
	}

	vrt_group_unref(group);
    }

    reply->retval = EXA_SUCCESS;
}


static uint64_t distance(uint64_t offset1, uint64_t offset2)
{
    return offset1 > offset2 ? offset1 - offset2 : offset2 - offset1;
}


void vrt_stat_request_begin(struct vrt_request *request)
{
    size_t nbsect = BYTES_TO_SECTORS(request->ref_bio->size);
    uint64_t sect = request->ref_bio->start_sector;
    exa_bool_t is_seq = TRUE;

    is_seq = (sect == request->ref_vol->stats.begin.next_sector &&
	      request->iotype == request->ref_vol->stats.begin.prev_request_type)
	|| (request->ref_vol->stats.begin.prev_request_type == VRT_IO_TYPE_NONE);

    EXA_ASSERT(VRT_IO_TYPE_IS_VALID(request->iotype));
    switch (request->iotype)
    {
    case VRT_IO_TYPE_READ:
	request->ref_vol->stats.begin.info.nb_sect_read += nbsect;
	++request->ref_vol->stats.begin.info.nb_req_read;

	if (!is_seq)
	{
	    request->ref_vol->stats.begin.info.nb_seek_dist_read +=
		distance(sect, request->ref_vol->stats.begin.next_sector);
	    ++request->ref_vol->stats.begin.info.nb_seeks_read;
	}

	break;

    case VRT_IO_TYPE_WRITE:
    case VRT_IO_TYPE_WRITE_BARRIER:
	request->ref_vol->stats.begin.info.nb_sect_write += nbsect;
	++request->ref_vol->stats.begin.info.nb_req_write;

	if (!is_seq)
	{
	    request->ref_vol->stats.begin.info.nb_seek_dist_write +=
		distance(sect, request->ref_vol->stats.begin.next_sector);
	    ++request->ref_vol->stats.begin.info.nb_seeks_write;
	}

	break;

    case VRT_IO_TYPE_NONE:
	EXA_ASSERT_VERBOSE(false, "Unexpected request type 'none'");
    }

    request->ref_vol->stats.begin.prev_request_type = request->iotype;
    request->ref_vol->stats.begin.next_sector = sect + nbsect;
}


void vrt_stat_request_done(struct vrt_request *request, bool failed)
{
    size_t nbsect = BYTES_TO_SECTORS(request->ref_bio->size);
    struct vrt_stats_done *info = &request->ref_vol->stats.done.info;

    if (failed)
    {
	++info->nb_req_err;
        return;
    }

    EXA_ASSERT_VERBOSE(VRT_IO_TYPE_IS_VALID(request->iotype),
                       "Unknown request type %d", request->iotype);
    switch (request->iotype)
    {
        case VRT_IO_TYPE_READ:
            info->nb_sect_read += nbsect;
            ++info->nb_req_read;
            break;

        case VRT_IO_TYPE_WRITE:
        case VRT_IO_TYPE_WRITE_BARRIER:
            info->nb_sect_write += nbsect;
            ++info->nb_req_write;
            break;

        case VRT_IO_TYPE_NONE:
            EXA_ASSERT(false);
    }
}
