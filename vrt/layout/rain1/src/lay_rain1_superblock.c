/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/layout/rain1/src/lay_rain1_superblock.h"
#include "vrt/layout/rain1/src/lay_rain1_desync_info.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/layout/rain1/src/lay_rain1_rdev.h"

#include "log/include/log.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

static uint64_t rain1_rdev_serialized_size(const rain1_realdev_t *lr)
{
    return sizeof(rain1_rdev_header_t);
}

static int rain1_rdev_serialize(const rain1_realdev_t *lr, stream_t *stream)
{
    rain1_rdev_header_t header;
    int w;

    header.uuid = lr->uuid;
    header.sync_tag = lr->sync_tag;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    return 0;
}

int rain1_rdev_header_read(rain1_rdev_header_t *header, stream_t *stream)
{
    int r = stream_read(stream, header, sizeof(rain1_rdev_header_t));

    if (r < 0)
        return r;
    else if (r != sizeof(rain1_rdev_header_t))
        return -EIO;

    return 0;
}

static int rain1_rdev_deserialize(rain1_realdev_t **lr, const rain1_group_t *rxg,
                                     const storage_t *storage, stream_t *stream)
{
    rain1_rdev_header_t header;
    vrt_realdev_t *rdev;
    int err;

    err = rain1_rdev_header_read(&header, stream);
    if (err != 0)
        return err;

    rdev = storage_get_rdev(storage, &header.uuid);
    if (rdev == NULL)
        return -VRT_ERR_SB_CORRUPTION;

    *lr = rain1_alloc_rdev_layout_data(rdev);
    if (*lr == NULL)
        return -ENOMEM;

    (*lr)->sync_tag = header.sync_tag;

    return 0;
}

int rain1_header_read(rain1_header_t *header, stream_t *stream)
{
    int r = stream_read(stream, header, sizeof(rain1_header_t));

    if (r < 0)
        return r;
    else if (r != sizeof(rain1_header_t))
        return -EIO;

    return 0;
}

uint64_t rain1_group_serialized_size(const rain1_group_t *rxg)
{
    uint64_t total_rdev_size;
    int i;

    total_rdev_size = 0;
    for (i = 0; i < rxg->nb_rain1_rdevs; i++)
        total_rdev_size += rain1_rdev_serialized_size(rxg->rain1_rdevs[i]);

    return sizeof(rain1_header_t)
           + total_rdev_size
           + assembly_group_serialized_size(&rxg->assembly_group);
}

int rain1_group_serialize(const rain1_group_t *rxg, stream_t *stream)
{
    rain1_header_t header;
    int w, i, err;

    header.magic = RAIN1_HEADER_MAGIC;
    header.blended_stripes = rxg->blended_stripes;
    header.su_size = rxg->su_size;
    header.max_sectors = rxg->max_sectors;
    header.sync_tag = rxg->sync_tag;
    header.logical_slot_size = rxg->logical_slot_size;
    header.dirty_zone_size = rxg->dirty_zone_size;
    exa_nodeset_copy(&header.nodes_resync, &rxg->nodes_resync);
    exa_nodeset_copy(&header.nodes_update, &rxg->nodes_update);
    header.nb_rdevs = rxg->nb_rain1_rdevs;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    for (i = 0; i < rxg->nb_rain1_rdevs; i++)
    {
        err = rain1_rdev_serialize(rxg->rain1_rdevs[i], stream);
        if (err < 0)
            return err;
    }

    return assembly_group_serialize(&rxg->assembly_group, stream);
}

int rain1_group_deserialize(rain1_group_t **rxg, const storage_t *storage,
                               stream_t *stream)
{
    rain1_header_t header;
    int i, err;
    assembly_volume_t *s;

    err = rain1_header_read(&header, stream);
    if (err != 0)
        return err;

    if (header.magic != RAIN1_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    *rxg = rain1_group_alloc();
    if (*rxg == NULL)
        return -ENOMEM;

    (*rxg)->blended_stripes = header.blended_stripes;
    (*rxg)->su_size = header.su_size;
    (*rxg)->max_sectors = header.max_sectors;
    (*rxg)->sync_tag = header.sync_tag;
    (*rxg)->logical_slot_size = header.logical_slot_size;
    (*rxg)->dirty_zone_size = header.dirty_zone_size;
    exa_nodeset_copy(&(*rxg)->nodes_resync, &header.nodes_resync);
    exa_nodeset_copy(&(*rxg)->nodes_update, &header.nodes_update);

    (*rxg)->nb_rain1_rdevs = header.nb_rdevs;

    for (i = 0; i < header.nb_rdevs; i++)
        (*rxg)->rain1_rdevs[i] = NULL;

    for (i = 0; i < header.nb_rdevs; i++)
    {
        rain1_realdev_t *lr = NULL;
        err = rain1_rdev_deserialize(&lr, (*rxg), storage, stream);
        if (err < 0)
            goto failed;
        (*rxg)->rain1_rdevs[lr->rdev->index] = lr;
    }

    err = assembly_group_deserialize(&(*rxg)->assembly_group, storage, stream);
    if (err != 0)
        goto failed;

    /* XXX I really do not know where to put this... It would have its place
     * in rain1_subspace deserialize, but there is no rain1_subspace structure
     * for now... */
    for (s = (*rxg)->assembly_group.subspaces; s != NULL; s = s->next)
        for (i = 0; i < s->total_slots_count; i++)
            s->slots[i]->private = slot_desync_info_alloc((*rxg)->sync_tag);

    return 0;

failed:
    EXA_ASSERT(err != 0);

    rain1_group_free(*rxg, storage);

    return err;
}
