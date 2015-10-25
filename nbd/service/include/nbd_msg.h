/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __NBD_MSG_H__
#define __NBD_MSG_H__

/** \file nbd_msg.h
 * \brief Private nbd service messages. */

#include "examsg/include/examsg.h"
#include "nbd/service/include/nbdservice_client.h"

struct nbd_request
{
  enum {
#define NBDCMD_FIRST NBDCMD_QUIT
    NBDCMD_QUIT = 300,
    NBDCMD_STATS,
    NBDCMD_DEVICE_DOWN,
    NBDCMD_DEVICE_SUSPEND,
    NBDCMD_DEVICE_RESUME,
    NBDCMD_DEVICE_EXPORT,
    NBDCMD_DEVICE_UNEXPORT,
    NBDCMD_DEVICE_IMPORT,
    NBDCMD_DEVICE_REMOVE,
    NBDCMD_SESSION_OPEN,
    NBDCMD_SESSION_CLOSE,
    NBDCMD_ADD_CLIENT,
    NBDCMD_REMOVE_CLIENT,
    NBDCMD_NDEV_INFO,
#define NBDCMD_LAST NBDCMD_NDEV_INFO
  } event;
#define NBDCMD_IS_VALID(c) ((c) >= NBDCMD_FIRST && (c) <= NBDCMD_LAST)

  char node_name[EXA_MAXSIZE_HOSTNAME + 1];
  exa_uuid_t device_uuid;
  char device_path[EXA_MAXSIZE_DEVPATH + 1];
  char net_id[EXA_MAXSIZE_NICADDRESS + 1];
  exa_nodeid_t node_id;
  int device_nb;
  uint64_t device_sectors;
  int major;
  int minor;
  int status;
  bool stats_reset;
};


typedef struct nbd_request nbd_request_t;

struct nbd_answer
{
  int status;
};

typedef struct nbd_answer nbd_answer_t;

struct nbd_majorminor
{
  /* major of the ndev */
  int ndev_major;
  /* minor of the ndev */
  int ndev_minor;
  /* request for major/minor succeeded or not */
  int status;
};

typedef struct nbd_majorminor nbd_majorminor_t;


/**
 * This message allows to ask the NBD server to lock or unlock write
 * requests for a specific range of a disk.
 */
EXAMSG_DCLMSG(ExamsgNbdLock, struct {
  /** UUID of the disk */
  exa_uuid_t disk_uuid;
  /** Starting sector of the area which must be locked/unlocked */
  uint64_t locked_zone_start;
  /** Size in sectors of the area to lock */
  uint64_t locked_zone_size;
  /** Boolean that tells whether it's a lock operation (true) or an
      unlock operation (false) */
  bool lock;
});

#endif /* __NBD_MSG_H__ */
