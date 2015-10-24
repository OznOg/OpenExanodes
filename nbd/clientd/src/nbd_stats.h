/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef _NBD_CLIENTD_NBD_STATS_H
#define _NBD_CLIENTD_NBD_STATS_H

#include "nbd/clientd/include/nbd_clientd.h"

#include "nbd/common/nbd_common.h"

#include "common/include/exa_constants.h"

#include "os/include/os_thread.h"

struct nbd_stats_begin
{
    uint64_t nb_sect_read;
    uint64_t nb_sect_write;
    uint64_t nb_req_read;
    uint64_t nb_req_write;
    uint64_t nb_seeks_read;
    uint64_t nb_seeks_write;
    uint64_t nb_seek_dist_read;
    uint64_t nb_seek_dist_write;
};


struct nbd_stats_done
{
    uint64_t nb_sect_read;
    uint64_t nb_sect_write;
    uint64_t nb_req_read;
    uint64_t nb_req_write;
    uint64_t nb_req_err;
};


struct nbd_stats_request
{
    bool reset;
    char node_name[EXA_MAXSIZE_HOSTNAME + 1];
    char disk_path[EXA_MAXSIZE_DEVPATH + 1];
    exa_uuid_t device_uuid;
};

struct device_stats
{
   /*
    * The data for nbd_stat_request_begin and nbd_stat_request_done have
    * to be protected by two separate mutexes, because if we used a
    * single mutex, the instrumentation would serialize sending and
    * receiving the requests (which are on separate threads). Having both
    * sides using their own mutex makes it so that the only competitor
    * for the mutex is nbd_stat_handle, when the statistics are
    * requested. This implies that the set of counters updated by each
    * function needs to be non-overlapping, unfortunately.
    */
    os_thread_mutex_t begin_mutex;
    os_thread_mutex_t done_mutex;

    /* The last time we reset the statistics (in ms). */
    uint64_t last_reset;

    struct {
        struct nbd_stats_begin info;
        uint8_t prev_request_type;
        uint64_t next_sector;
    } begin;

    struct {
        struct nbd_stats_done info;
    } done;
};

struct nbd_stats_reply
{
    uint64_t last_reset;
    uint64_t now;
    struct nbd_stats_begin begin;
    struct nbd_stats_done done;
};

void nbd_stat_init(struct device_stats *stats);
void nbd_stat_clean(struct device_stats *stats);
void nbd_stat_restart(struct device_stats *stats);

void nbd_get_stats(struct device_stats *stats, struct nbd_stats_reply *reply,
                   bool reset);

void nbd_stat_request_begin(struct device_stats *stats, const nbd_io_desc_t *req);
void nbd_stat_request_done(struct device_stats *stats, const nbd_io_desc_t *req_header);

#endif /* _NBD_CLIENTD_NBD_STATS_H */
