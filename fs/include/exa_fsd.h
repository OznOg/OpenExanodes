/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXAFSD
#define H_EXAFSD

/** \file exa_fsd.h
 * \brief Filesystem daemon.
 *
 * Public header for communication with the filesystem daemon
 */

#include "common/include/exa_constants.h"
#include "examsg/include/examsg.h"

#include "fs/include/fs_data.h"

/* === Client API ==================================================== */

/*
 * See doxygen documentation
 */

/* is_fs_mounted */
int fsd_is_fs_mounted(ExamsgHandle mh, const char* devpath);

/* is_mountpoint_used */
int fsd_is_mountpoint_used(ExamsgHandle mh, const char* mountpoint);

/* capacity of a filesystem */
int fsd_df(ExamsgHandle mh, const char* mountpoint, struct fsd_capa *capa);

/* create a filesystem */
int fsd_fs_create_local(ExamsgHandle mh, const char* fstype, const char* devpath);
int fsd_fs_create_gfs(ExamsgHandle mh, const fs_data_t* fs);

/* prepare: load daemons & modules */
int fsd_prepare(ExamsgHandle mh,  const fs_data_t* fs);

/* update config : tell GFS that a config change occurred. */
int fsd_gfs_update_config(ExamsgHandle mh);

/* mount a filesystem */
int fsd_mount(ExamsgHandle mh, const fs_data_t* fs, int read_only, int mount_remount,
	      const char *group_name, const char *fsname);

/* umount a filesystem */
int fsd_umount(ExamsgHandle mh, const fs_data_t* fs,
	       const char *group_name, const char *fsname);

/* unload daemons & modules of a filesystem */
int fsd_unload(ExamsgHandle mh,  const fs_data_t* fs);

/* prepare the resize of a filesystem */
int fsd_prepare_resize(ExamsgHandle mh, const char* fstype, const char* devpath);

/* resize a filesystem */
int fsd_resize(ExamsgHandle mh, const char* fstype, const char* devpath,
	       const char* mountpoint, const uint64_t sizeKB);

/* finalize the resizing of a filesystem (or set it back to what it was) */
int fsd_finalize_resize(ExamsgHandle mh, const char* fstype, const char* devpath);

/* Copy of a filesystem : copy all files from source to destination */
int fsd_copy(ExamsgHandle mh, const char* source_path, const char* destination_path);

/* FS check */
int fsd_check(ExamsgHandle mh, const fs_data_t* fs,
	      const char* optional_parameter,
	      bool repair,
	      const char* output_file);

/* Tell a change to CMAN */
int fsd_cman_update(ExamsgHandle mh, const char* nodename);

/* Increase the number of logs in GFS */
int fsd_set_gfs_logs(ExamsgHandle mh, const fs_data_t* fs, int number_of_logs);

/* Update the different tuned value for a SFS, if it's mounted. */
int fsd_update_gfs_tuning(ExamsgHandle mh, const fs_data_t* fs);

#endif

