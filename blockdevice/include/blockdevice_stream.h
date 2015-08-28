/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef BLOCKDEV_STREAM_H
#define BLOCKDEV_STREAM_H

#include "blockdevice/include/blockdevice.h"

/* FIXME vrt_stream should be move out from VRT */
#include "vrt/common/include/vrt_stream.h"

#include "os/include/os_file.h"
#include "os/include/os_inttypes.h"

/**
 * Open a stream on an already opened block device.
 *
 * WARNING - This function allocates memory.
 *
 * @param[out] stream             Stream created
 * @param[in]  bdev               Block device. The device is *not* closed
 *                                when the stream is closed.
 * @param[in]  cache_size         Cache size, in bytes
 * @param[in]  access             Access mode
 *
 * @return stream if successful, NULL otherwise
 */
int blockdevice_stream_on(stream_t **stream, blockdevice_t *bdev,
                          size_t cache_size, stream_access_t access);

#endif /* BLOCKDEV_STREAM_H */
