/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_FS_H
#define __ADM_FS_H


#include "common/include/exa_constants.h"

#include "fs/include/fs_data.h"

struct adm_volume;

struct adm_fs
{
  uint64_t size;
  char type[EXA_MAXSIZE_FSTYPE + 1];
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT + 1];
  char gfs_uuid[GFS_UUID_STR_SIZE];       /**< UUID used at mount time */
  uint32_t gfs_nb_logs;                   /**< Number of logs in the FS. */
  uint64_t gfs_readahead;                 /**< Set by the fstune command */
  uint32_t gfs_demote_secs;               /**< Set by the fstune command */
  uint32_t gfs_glock_purge;               /**< Set by the fstune command */
  uint64_t gfs_rg_size;                   /**< Set at creation, in MB */
  bool gfs_fuzzy_statfs;            /**< fuzzy_statfs for fast "df", slow "mount" */
  char mount_option[EXA_MAXSIZE_MOUNT_OPTION + 1];
  int committed;
  struct adm_volume *volume;
  int offline;
};

struct adm_fs *adm_fs_alloc(void);

void __adm_fs_free(struct adm_fs *fs);
#define adm_fs_free(fs) (__adm_fs_free(fs), fs = NULL)

#endif /* __ADM_FS_H */
