/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef _NBD_SERVERD_NBD_SERVERD_H
#define _NBD_SERVERD_NBD_SERVERD_H

#include "nbd/common/nbd_common.h"
#include "common/include/exa_constants.h"
#include "os/include/os_semaphore.h"
#include "rdev/include/exa_rdev.h"

#ifdef WITH_PERF
#include "exaperf/include/exaperf.h"
#endif

#include "common/include/uuid.h"
#include "common/include/exa_nodeset.h"

#include "examsg/include/examsg.h"

/* a structure which contains all necessary information on a disk
 * managed by the server */
struct device
{
  /* UUID of the disk device */
  exa_uuid_t uuid;
  /* path to the disk device */
  char path[EXA_MAXSIZE_DEVPATH + 1];
  exa_rdev_handle_t *handle;
  /* List of incoming requests (interface with 'ti_main' thread) */
  struct nbd_list disk_queue;
  /* Size of the device in sectors
   * FIXME: An accessor to rdev would be more suitable
   */
  uint64_t size_in_sectors;

  uint32_t dev_index;

  struct locked_zone {
      uint64_t sector;
      uint64_t sector_count;
  } locked_zone[NBMAX_DISK_LOCKED_ZONES];
  int nb_locked_zone;

  bool exit_thread;

  /* used for lock/unlocking of zone */
  int locking_return;
  os_sem_t lock_sem_disk;

#ifdef WITH_PERF
    exaperf_sensor_t *rdev_dur[2];
    exaperf_sensor_t *inter_arrival_repart[2];
    uint64_t last_req_time[2];
#endif
};

typedef struct device device_t;

struct server
{
  volatile bool run;

  /* id of the server */
  int server_id;
  /* local devices managed by the server */
  device_t *devices[NBMAX_DISKS_PER_NODE];

  /* protects devices array */
  os_thread_mutex_t mutex_edevs;

  /* name and id of the node the server is running on */
  char *node_name;
  exa_nodeid_t node_id;

  nbd_tcp_t *tcp;

  /* server side threads of the NBD */
  os_thread_t td_pid[NBMAX_DISKS_PER_NODE];
  os_thread_t teh_pid;
  os_thread_t rebuild_helper_id;

  struct nbd_root_list list_root;

  /* queue of buffers managed by TI to handle requests server side */
  struct nbd_root_list ti_queue;

  /* maximum number of receivable headers */
  int num_receive_headers;

  /* Examsg mail box */
  ExamsgHandle mh;

  /* semaphore to wait for threads to create their mailbox */
  os_sem_t mailbox_sem;

  int bd_buffer_size;

  struct daemon_request_queue *server_requests_queue;

  /* file descriptor of the exa_rdev module */
  int exa_rdev_fd;
};

typedef struct server server_t;

extern server_t nbd_server;

void nbd_server_send(const nbd_io_desc_t *io);

void nbd_server_end_io(header_t *req_header);

#endif /* _NBD_SERVERD_NBD_SERVERD_H */
