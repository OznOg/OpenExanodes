/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef NBD_COMMON
#define NBD_COMMON

#include "common/include/exa_error.h"
#include "common/include/exa_nbd_list.h"
#include "common/include/exa_nodeset.h"

#define DEFAULT_BD_BUFFER_SIZE 131072
typedef struct {
    enum {
        NBD_REQ_TYPE_READ = 236,
        NBD_REQ_TYPE_WRITE
    } request_type;
#define NBD_REQ_TYPE_IS_VALID(type) \
    ((type) == NBD_REQ_TYPE_READ || (type) == NBD_REQ_TYPE_WRITE)

    uint64_t sector;
    uint32_t sector_nb;
    int8_t disk_id;
    uint64_t req_num;
    int8_t result;
    bool bypass_lock;     /**< tells if the IO can bypass rebuilding lock */
    bool flush_cache;     /**< tells if the IO needs a disk cache synchronization (barrier) */
    void *buf;
} __attribute__((__packed__)) nbd_io_desc_t;

#define NBD_HEADER_NET_SIZE (sizeof(nbd_io_desc_t) - sizeof(void *))

typedef struct tcp_plugin tcp_plugin_t;

struct nbd_tcp
{
  /* this function can be used to used buffer internally allocated by
   * plugin */

  void (*end_sending)(const nbd_io_desc_t *io, int error);
  void (*end_receiving)(exa_nodeid_t from, const nbd_io_desc_t *io, int error);

  void *(*get_buffer)(const nbd_io_desc_t *io);
  /* function called by plugin to get the buffer of the plugin */

  /* Internal structure of the plugin */
  tcp_plugin_t *tcp;
};

typedef struct nbd_tcp nbd_tcp_t;

#endif
