/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/layout/sstriping/src/lay_sstriping.h"
#include "vrt/layout/sstriping/src/lay_sstriping_group.h"
#include "vrt/layout/sstriping/src/lay_sstriping_superblock.h"

#include "log/include/log.h"

#include "common/include/exa_error.h"

#include "os/include/os_error.h"

int sstriping_header_read(sstriping_header_t *header, stream_t *stream)
{
    int r = stream_read(stream, header, sizeof(sstriping_header_t));

    if (r < 0)
        return r;
    else if (r != sizeof(sstriping_header_t))
        return -EIO;

    return 0;
}

uint64_t sstriping_group_serialized_size(const sstriping_group_t *ssg)
{
    return sizeof(sstriping_header_t)
         + assembly_group_serialized_size(&ssg->assembly_group);
}

int sstriping_group_serialize(const sstriping_group_t *ssg, stream_t *stream)
{
    sstriping_header_t header;
    int w;

    header.magic = SSTRIPING_HEADER_MAGIC;
    header.su_size = ssg->su_size;
    header.logical_slot_size = ssg->logical_slot_size;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    return assembly_group_serialize(&ssg->assembly_group, stream);
}

int sstriping_group_deserialize(sstriping_group_t **ssg, const storage_t *storage,
                                   stream_t *stream)
{
    sstriping_header_t header;
    int r;
    int err;

    r = stream_read(stream, &header, sizeof(header));
    if (r < 0)
        return r;
    else if (r != sizeof(header))
        return -EIO;

    if (header.magic != SSTRIPING_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    *ssg = sstriping_group_alloc();
    if (*ssg == NULL)
        return -ENOMEM;

    (*ssg)->su_size = header.su_size;
    (*ssg)->logical_slot_size = header.logical_slot_size;

    err = assembly_group_deserialize(&(*ssg)->assembly_group, storage, stream);
    if (err != 0)
        goto failed;

    return 0;

failed:
    EXA_ASSERT(err != 0);

    sstriping_group_free(*ssg, storage);

    return err;
}
