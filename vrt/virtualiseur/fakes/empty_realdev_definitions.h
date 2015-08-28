/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_REALDEV_FAKES_H__
#define __UT_VRT_REALDEV_FAKES_H__

/*** FIXME FAKES BECAUSE THE WHOLE THING IS CRAP AND IT'S NOT POSSIBLE TO
   BUILD THIS SIMPLE UNIT TEST WITHOUT DRAGGING IN THE WHOLE RETARDED
   UNIVERSE ***/
uint64_t rdev_chunk_based_size(const vrt_realdev_t *rdev) { return 0; }

vrt_realdev_t *vrt_rdev_new(exa_nodeid_t node_id,
                                 spof_id_t spof_id, const exa_uuid_t *uuid,
                                 const exa_uuid_t *nbd_uuid, int index,
                                 bool local, bool up)
{
    return NULL;
}

int vrt_rdev_open(struct vrt_realdev *rdev)
{
    return 0;
}

void __vrt_rdev_free(struct vrt_realdev *rdev)
{
}

int vrt_rdev_replace(struct vrt_realdev *rdev, const exa_uuid_t *new_rdev_uuid)
{
    return 0;
}

int vrt_rdev_up(struct vrt_realdev *rdev)
{
    return 0;
}

void vrt_rdev_down(struct vrt_realdev *rdev)
{
}

exa_nodeid_t vrt_rdev_get_nodeid(const struct vrt_realdev *rdev)
{
    return EXA_NODEID_NONE;
}

exa_realdev_status_t rdev_get_compound_status(const struct vrt_realdev *rdev)
{
    return 0;
}

int vrt_rdev_lock_sectors(struct vrt_realdev *rdev, unsigned long start,
                          unsigned long end)
{
    return 0;
}
int vrt_rdev_unlock_sectors(struct vrt_realdev *rdev, unsigned long start,
                            unsigned long end)
{
    return 0;
}

uint64_t vrt_realdev_get_usable_size(const vrt_realdev_t *rdev)
{
    /* FIXME Where does these values come from?? */
    return rdev->real_size - 65 * 1024;
}

int vrt_rdev_create_superblocks(vrt_realdev_t *rdev)
{
    return 0;
}

int vrt_rdev_begin_superblock_write(vrt_realdev_t *rdev, uint64_t old_version,
                                    uint64_t new_version, superblock_write_op_t *op)
{
    return 0;
}

int vrt_rdev_end_superblock_write(vrt_realdev_t *rdev, superblock_write_op_t *op)
{
    return 0;
}

int vrt_rdev_begin_superblock_read(vrt_realdev_t *rdev, uint64_t sb_version,
                                   superblock_read_op_t *op)
{
    return 0;
}

int vrt_rdev_end_superblock_read(vrt_realdev_t *rdev, superblock_read_op_t *op)
{
    return 0;
}


#endif
