/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef NARROWED_STREAM_H
#define NARROWED_STREAM_H

#include "vrt/common/include/vrt_stream.h"

/**
 * Open a narrowed stream on a substream.
 *
 * A "narrowed" stream is a stream that restricts access to a substream
 * to a specified range. As such, only seekable and tellable streams may
 * be narrowed.
 *
 * WARNING - This function allocates memory.
 *
 * @param[out] narrow_stream  Narrowed stream created
 * @param      base_stream    Stream to be narrowed
 * @param[in]  start          Start offset
 * @param[in]  end            End offset
 * @param[in]  access         Access mode
 *
 * NOTES:
 *
 *  - No check is performed on start and end offsets.
 *
 *  - The closing of a narrow stream does *not* close its base stream.
 *
 *  - The valid offset range on the narrowed stream is [0..end-start].
 *    Attempting to write beyond the end of the valid range returns -ENOSPC.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int narrowed_stream_open(stream_t **narrow_stream, stream_t *base_stream,
                         uint64_t start, uint64_t end, stream_access_t access);

#endif /* NARROWED_STREAM_H */
