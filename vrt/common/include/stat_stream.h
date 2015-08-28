/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef STAT_STREAM_H
#define STAT_STREAM_H

#include "vrt/common/include/vrt_stream.h"

/* TODO (?) Use exaperf internally? Or write an exaperf_stream? */

/** Statistics on a single stream operation */
typedef struct
{
    uint32_t op_count;     /**< Number of times the operation was performed */
    uint32_t error_count;  /**< Number of errors */
    uint64_t total_bytes;  /**< Total number of bytes processed */
} stream_op_stats_t;

/** Stats for all operations on a stream */
typedef struct
{
    /* Note:
       - for flush_stats, total_bytes is irrelevant
       - for seek_stats, total_bytes is the sum of all absolute seeks
       - for tell_stats, total_bytes is irrelevant. */
    stream_op_stats_t read_stats;
    stream_op_stats_t write_stats;
    stream_op_stats_t flush_stats;
    stream_op_stats_t seek_stats;
    stream_op_stats_t tell_stats;
} stream_stats_t;

/**
 * Open a statistics stream on an existing stream.
 *
 * WARNING - This function allocates memory.
 *
 * NOTES:
 *
 * - Closing a stat stream does *not* close the substream.
 *
 * - The stats only count operations performed through the stream API, which
 *   means that the number of times an operation was issued may be higher
 *   than that reported by the stats.
 *
 *   For instance, if an implementation performs seeks internally without
 *   calling stream_seek(), the actual number of seeks performed will be
 *   higher than reported, as the stats only account for the seeks performed
 *   by calling stream_seek().
 *
 * @param[out] stat_stream  Stat stream created
 * @param      base_stream  Base stream
 * @param      stats        Statistics (contents valid even after the stats stream have
 *                          been closed)
 *
 * @return 0 if successful, a negative error code otherwise
 */
int stat_stream_open(stream_t **stat_stream, stream_t *base_stream,
                     stream_stats_t *stats);

#endif /* STAT_STREAM_H */
