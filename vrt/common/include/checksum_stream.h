/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef CHECKSUM_STREAM_H
#define CHECKSUM_STREAM_H

#include "vrt/common/include/vrt_stream.h"

#include "common/include/checksum.h"

/**
 * Open a checksum stream on an existing stream.
 *
 * WARNING - This function allocates memory.
 *
 * @param[out] stream          Checksum stream created
 * @param[in]  base_stream     Stream the checksum stream will wrap
 *
 * @return 0 if successful, a negative error code otherwise
 *
 * @note stream_seek is not supported on checksum streams only the rewind is
 */
int checksum_stream_open(stream_t **stream, stream_t *base_stream);


/**
 * Get the checksum value corresponding to the stream
 *
 * @param[in] stream  The checksum stream
 *
 * @return the checksum
 */
checksum_t checksum_stream_get_value(const stream_t *stream);

/**
 * Get the size of checksumed data corresponding to the stream
 *
 * @param[in] stream  The checksum stream
 *
 * @return the size
 */
size_t checksum_stream_get_size(const stream_t *stream);

/**
 * Reset the checksum corresponding to the stream
 *
 * @param[in] stream  The checksum stream
 *
 * @note a reset is done automatically at rewind
 */
void checksum_stream_reset(stream_t *stream);

#endif /* CHECKSUM_STREAM_H */
