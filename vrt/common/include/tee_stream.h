/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TEE_STREAM_H
#define TEE_STREAM_H

#include "vrt/common/include/vrt_stream.h"

/**
 * Create a "tee" stream, i.e. a stream that forks in two.
 *
 * WARNING - This function allocates memory.
 *
 * The tee stream is write only. The behaviour is the following:
 *
 *   - Both substreams must support writing.
 *
 *   - If an error is raised when writing on one of the substreams,
 *     this error is returned right away. No guarantee is given as
 *     to whether the other substream has been written to.
 *
 *   - The flushing is performed in a best effort manner: each substream
 *     is tentatively flushed regardless of whether the other substream
 *     raised an error. If an error occurred during the flushing of either
 *     of the substreams, it is returned (in case both substreams raised
 *     an error, no guarantee is given as to which substream it is from).
 *
 *   - If an error is raised when seeking on one of the substreams,
 *     this error is returned right away. No guarantee is given as
 *     to whether the other substream has been seeked.
 *
 *   - The tell operation returns the offset from one of the substreams.
 *     It is guaranteed to be the same on both of them *iff* all prior
 *     write and seek operations on the tee stream were successful, the
 *     first of these operations was a seek and the substreams were not
 *     seeked directly.
 *
 *   - Closing a tee stream does *not* close the substreams.
 *
 * @param[out] tee_stream  Tee stream created
 * @param[in] stream1      A stream
 * @param[in] stream2      A stream
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tee_stream_open(stream_t **tee_stream, stream_t *stream1, stream_t *stream2);

#endif /* TEE_STREAM_H */
