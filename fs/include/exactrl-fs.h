/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXACTRL_FS
#define H_EXACTRL_FS

/** \file exactrl-fs.h
 * \brief Public filesystem daemon messages.
 *
 */

#include "fs/include/exa_fsd.h"

/* === Requests type enum ============================================ */

typedef enum FSRequestType {
  FSREQUEST_FSINFO,                   /**< is the filesystem mounted? */
  FSREQUEST_MOUNTPOINTINFO,           /**< is the path mounted? */
  FSREQUEST_DF_INFO,                  /**< infos from "df" */
  FSREQUEST_MOUNT,                    /**< mount a filesystem */
  FSREQUEST_PREUMOUNT,                /**< call user script before umount */
  FSREQUEST_POSTMOUNT,                /**< call user script after mount */
  FSREQUEST_UMOUNT,                   /**< unmount a filesystem */
  FSREQUEST_PREPARE,                  /**< prepare a filesystem (i.e load daemons) */
  FSREQUEST_UNLOAD,                   /**< unload a filesystem */
  FSREQUEST_CREATEGFS,                /**< create a GFS filesystem */
  FSREQUEST_CREATELOCAL,              /**< create a local filesystem */
  FSREQUEST_RESIZE,                   /**< resize a filesystem */
  FSREQUEST_CHECK,                    /**< request for a check */
  FSREQUEST_UPDATE_GFS_CONFIG,        /**< The config file for GFS has changed */
  FSREQUEST_UPDATE_CMAN,              /**< Tell a change to CMAN */
  FSREQUEST_SET_LOGS,                 /**< Set logs of a GFS FS */
  FSREQUEST_UPDATE_GFS_TUNING         /**< Set read ahead on-the-fly of a GFS FS */
} FSRequestType;

/* === Examsg types for each request types =========================== */

/* ------------------------------------------------------------------- */
typedef struct FSRequestFSInfo {
  char devpath[EXA_MAXSIZE_DEVPATH+1];
} FSRequestFSInfo;

/* ------------------------------------------------------------------- */
typedef struct FSRequestMountpointInfo {
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT+1];
} FSRequestMountpointInfo;

/* ------------------------------------------------------------------- */
typedef struct FSRequestDfInfo {
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT+1];
} FSRequestDfInfo;

/* ------------------------------------------------------------------- */
/* Warning: must be 64-bit aligned */
#define FSD_MAXSIZE_ACTION 15
#if (FSD_MAXSIZE_ACTION+1) & 0x7
# error "FSD_MAXSIZE_ACTION+1 is not a multiple of 8 bytes"
#endif

/** used for prepare, mount, umount, unload */
typedef struct FSRequestStartStop {
  uint64_t read_only;                     /**< Do a mount RO or RW ? */
  uint64_t mount_remount;                 /**< Allowed to mount and/or remount. */
  fs_data_t fs;
  char group_name[EXA_MAXSIZE_GROUPNAME+1];
  char fs_name[EXA_MAXSIZE_VOLUMENAME+1];
} FSRequestStartStop;

/* ------------------------------------------------------------------- */
/** used to create a gfs filesystem */
typedef struct FSRequestCreateGfs {
  fs_data_t fs;
} FSRequestCreateGfs;

/* ------------------------------------------------------------------- */
/** used to create a local filesystem */
typedef struct FSRequestCreateLocal {
  char fstype[EXA_MAXSIZE_FSTYPE+1];      /**< fstype: ext3, ... */
  char devpath[EXA_MAXSIZE_DEVPATH+1];        /**< /dev/exa/group/volume */
} FSRequestCreateLocal;

/* ------------------------------------------------------------------- */
/** used to resize a filesystem */

typedef enum FSResizeType {
  FSRESIZE_PREPARE,                   /**< Do we prepare the resizing ? */
  FSRESIZE_RESIZE,                    /**< Do we do it ? */
  FSRESIZE_FINALIZE                   /**< Do we need to do finalizing actions ? (i.e : journal on ext3) */
} FSResizeType;

typedef struct FSRequestResize {
  char fstype[EXA_MAXSIZE_FSTYPE+1];         /**< fstype: ext3, gfs, ... */
  char devpath[EXA_MAXSIZE_DEVPATH+1];       /**< /dev/exa/group/volume */
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT+1]; /**< /dev/exa/group/volume */
  uint64_t action;                           /**< tristate: prepare/resize/finalize the resize.
					          Type FSResizeType */
  uint64_t sizeKB;                           /**< New size in KB or 0 to use the size of the device */
} FSRequestResize;

/* ------------------------------------------------------------------- */
/** used to check a filesystem */
typedef struct FSRequestCheck {
  fs_data_t fs;                                                 /**< Data for FS to be checked */
  char optional_parameters[EXA_MAXSIZE_FSCHECK_PARAMETER+1];    /**< "-F -..." */
  char output_file[EXA_MAXSIZE_MOUNTPOINT+1];                   /**< /tmp/output_fsck.5559985 */
  bool repair;                                                  /**< Need to repair ? */
} FSRequestCheck;

/* ------------------------------------------------------------------- */
/** tell a change to CMAN */
typedef struct FSRequestUpdateCman {
  char nodename[EXA_MAXSIZE_HOSTNAME+1];                            /**< sam2584.toulouse */
} FSRequestUpdateCman;

/* ------------------------------------------------------------------- */
/** Increase the number of logs in GFS */
typedef struct FSRequestSetLogs {
  fs_data_t fs;                           /**< Data for FS whose number of logs must change */
  int number_of_logs;
} FSRequestSetLogs;

/* ------------------------------------------------------------------- */
/** Set the tuning values for a particular GFS */
typedef struct FSRequestUpdateGfsTuning {
  fs_data_t fs;                           /**< Data for FS for which tuning options must be set */
} FSRequestUpdateGfsTuning;

/* === Union of all examsg request type ============================== */

typedef struct FSRequest
{
  FSRequestType requesttype;
  union
  {
    FSRequestFSInfo fsinfo;
    FSRequestMountpointInfo mountpointinfo;
    FSRequestDfInfo dfinfo;
    FSRequestStartStop startstop;
    FSRequestCreateGfs creategfs;
    FSRequestCreateLocal createlocal;
    FSRequestResize resize;
    FSRequestCheck check;
    FSRequestUpdateCman updatecman;
    FSRequestSetLogs setlogs;
    FSRequestUpdateGfsTuning updategfstuning;
  } argument;
} FSRequest;

/* === Struct of DfInfo answer ======================================= */

typedef struct FSAnswer {
  union {
    struct fsd_capa capa;
    int number_of_logs;
  };
  int64_t ack;
  FSRequestType request;
} FSAnswer;

#endif /* H_EXACTRL-FS */
