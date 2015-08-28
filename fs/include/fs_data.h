/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_FS_DATA
#define H_FS_DATA

/** \file fs_definition.h
 * \brief Common file system structure definitions shared between
 * exa_fsd and admind
 *
 */

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

/** Allow to issue a simple "mount" or "mount -o remount" command.
    These are set as a bitfield */

#define  FS_ALLOW_MOUNT        1
#define  FS_ALLOW_REMOUNT      2

/* Defining the different sizes for a FS. ("df" on a filesystem will return this) */
struct fsd_capa {
  int64_t size; /* size of the filesystem in bytes */
  int64_t used; /* size used of the filesystem in bytes */
  int64_t free; /* free space on the filesystem in bytes */
};

/** GFS infos in the fs XML node */
typedef struct fs_gfs_data {
  char lock_protocol[EXA_MAXSIZE_GFS_LOCK_PROTO+1];       /**< lock_gulm / lock_dlm */
#define GFS_UUID_STR_SIZE (GFS_SIZE_STRING_CLUSTER_FS_ID*2+8) /**< + ':' +'\0' + enforce padding */
  char uuid[GFS_UUID_STR_SIZE];       /**< The UUID that GFS must use for this FS. */
  uint64_t nb_logs;                   /**< How many nodes can mount the file system concurrently ? */
  uint64_t read_ahead;                /**< Setting for read-ahead, modified at mount time. */
  uint64_t rg_size;                   /**< Setting for RG size, set at fscreate. */
  uint64_t fuzzy_statfs;              /**< fuzzy_statfs for fast "df", slow "mount" */
  uint32_t demote_secs;               /**< delay in seconds before demoting gfs write locks. */
  uint32_t glock_purge;               /**< percentage of unused glocks to trim every 5 seconds */
} fs_gfs_data_t;

/** clustered infos in the fs XML node */
typedef struct fs_clustered_data {
  union {
    struct fs_gfs_data gfs;
  };
} fs_clustered_data_t;

/** Generic infos in the fs XML node */
typedef struct fs_data {
  uint64_t sizeKB;                          /**< Size of the filesystem */
  char fstype[EXA_MAXSIZE_FSTYPE+1];        /**< Type of the filesystem (ext3...) */
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT+1];/**< /mnt/ext */
  uint64_t transaction;                     /**< Transaction : 1 COMMITTED, 0 INPROGRESS */
  exa_nodeset_t goal_started;               /**< Which nodes have goal to start this FS, RW _and_ RO nodes. */
  exa_nodeset_t goal_started_ro;            /**< Which nodes have goal to start this FS, in RO mode
					         WARNING! This list is a subset of goal_started. */
  exa_uuid_t volume_uuid;
  char devpath[EXA_MAXSIZE_DEVPATH+1];        /**< /dev/exa/group/volume */
  char mount_option[EXA_MAXSIZE_MOUNT_OPTION + 1];
  union {
    struct fs_clustered_data clustered;
  };
} fs_data_t;

#endif

