/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SERVICE_FS__H
#define __SERVICE_FS__H

#include "admind/services/fs/generic_fs.h"
#include "fs/include/fs_data.h"

struct fs_info_reply
{
  struct fsd_capa capa;                      /**< rdev volume info  */
  uint64_t mounted;                          /**< boolean, is the filesystem mounted on this node */
};

exa_error_code fs_update_tree(int thr_nb, fs_data_t*);

/**
 *  Define a FS specific iterator
 *
 *  A file system is a DEVICE and additional properties.
 *  We must iterate on all of them independantly.
 *  We can iterate on them depending on different properties : type, started, valid, etc.
 **/

struct adm_fs;

typedef struct fs_iterator
{
  struct adm_fs* fs;
  fs_data_t fs_data;
} fs_iterator_t;

void init_fs_iterator(fs_iterator_t* iter);
fs_data_t* iterate_fs_instance(fs_iterator_t* iter, bool valid, const char* type, bool started);

/* Needed here for clinfo */
extern uint64_t fs_handle_gfs;

/* Names of fstune options */
#define EXA_FSTUNE_GFS_LOGS         "sfs_logs"
#define EXA_FSTUNE_MOUNT_OPTION     "mount_option"
#define EXA_FSTUNE_READAHEAD        "readahead"
#define EXA_FSTUNE_DEMOTE_SECS      "demote_secs"
#define EXA_FSTUNE_DEMOTE_SECS_MAX  900
#define EXA_FSTUNE_GLOCK_PURGE      "glock_purge"
#define EXA_FSTUNE_GFS_RG_SIZE      "sfs_rg_size"
#define EXA_FSTUNE_GFS_FUZZY_STATFS "sfs_fuzzy_statfs"

#endif
