/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/stat_stream.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

typedef struct
{
    stream_t *base_stream;
    stream_stats_t *stats;
} stat_stream_context_t;

static int stat_stream_read(void *v, void *buf, size_t size)
{
    stat_stream_context_t *ssc = v;
    int r;

    r = stream_read(ssc->base_stream, buf, size);

    ssc->stats->read_stats.op_count++;
    if (r < 0)
        ssc->stats->read_stats.error_count++;
    else
        ssc->stats->read_stats.total_bytes += r;

    return r;
}

static int stat_stream_write(void *v, const void *buf, size_t size)
{
    stat_stream_context_t *ssc = v;
    int w;

    w = stream_write(ssc->base_stream, buf, size);

    ssc->stats->write_stats.op_count++;
    if (w < 0)
        ssc->stats->write_stats.error_count++;
    else
        ssc->stats->write_stats.total_bytes += w;

    return w;
}

static int stat_stream_flush(void *v)
{
    stat_stream_context_t *ssc = v;
    int err;

    err = stream_flush(ssc->base_stream);

    ssc->stats->flush_stats.op_count++;
    if (err != 0)
        ssc->stats->flush_stats.error_count++;

    return err;
}

static int stat_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    stat_stream_context_t *ssc = v;
    int s;

    s = stream_seek(ssc->base_stream, offset, seek);

    ssc->stats->seek_stats.op_count++;
    if (s < 0)
        ssc->stats->seek_stats.error_count++;
    else
        ssc->stats->seek_stats.total_bytes += (offset >= 0 ? offset : -offset);

    return s;
}

static uint64_t stat_stream_tell(void *v)
{
    stat_stream_context_t *ssc = v;
    uint64_t pos;

    pos = stream_tell(ssc->base_stream);

    ssc->stats->tell_stats.op_count++;
    if (pos == STREAM_TELL_ERROR)
        ssc->stats->tell_stats.error_count++;
    /* No byte stats for tell: no meaning */

    return pos;
}

static void stat_stream_close(void *ssc)
{
    os_free(ssc);
}

static stream_ops_t stat_stream_ops =
{
    .read_op = stat_stream_read,
    .write_op = stat_stream_write,
    .flush_op = stat_stream_flush,
    .seek_op = stat_stream_seek,
    .tell_op = stat_stream_tell,
    .close_op = stat_stream_close
};

static void op_stats_init(stream_op_stats_t *stats)
{
    stats->op_count = 0;
    stats->error_count = 0;
    stats->total_bytes = 0;
}

static void init_stats(stream_stats_t *stats)
{
    op_stats_init(&stats->read_stats);
    op_stats_init(&stats->write_stats);
    op_stats_init(&stats->flush_stats);
    op_stats_init(&stats->seek_stats);
    op_stats_init(&stats->tell_stats);
}

int stat_stream_open(stream_t **stat_stream, stream_t *base_stream,
                     stream_stats_t *stats)
{
    stat_stream_context_t *ssc;
    int err;

    if (base_stream == NULL || stats == NULL)
        return -EINVAL;

    ssc = os_malloc(sizeof(stat_stream_context_t));
    if (ssc == NULL)
        return -ENOMEM;

    ssc->base_stream = base_stream;
    ssc->stats = stats;

    init_stats(ssc->stats);

    err = stream_open(stat_stream, ssc, &stat_stream_ops,
                      stream_access(ssc->base_stream));
    if (err != 0)
    {
        os_free(ssc);
        return err;
    }

    return 0;
}
