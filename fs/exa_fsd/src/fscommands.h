/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_FSCOMMANDS
#define H_FSCOMMANDS

/* === Internal API ================================================== */

/* File included only in exa_fsd, do not include this file elsewhere */

int fsd_do_fsinfo(const char *devpath);
int fsd_do_mountpointinfo(const char *mountpoint);
int fsd_do_dfinfo(const char *mountpoint, struct fsd_capa *capa);

int fsd_do_mount(const fs_data_t* fs, int read_only, int mount_remount);
int fsd_do_post_mount(const fs_data_t* fs, const char *group_name,
		      const char *fs_name);
int fsd_do_pre_umount(const fs_data_t* fs, const char *group_name,
		      const char *fs_name);
int fsd_do_umount(const fs_data_t* fs);
int fsd_do_prepare(const fs_data_t* fs);
int fsd_do_unload(const fs_data_t* fs);

int fsd_do_creategfs(const fs_data_t* fs);

int fsd_do_createlocal(const char* fstype, const char* devpath);

int fsd_do_resize(const char* fstype, const char* devpath,
		  const char* mountpoint,
		  const int prepare,
		  const uint64_t sizeKB);

int fsd_do_copy(const char* devpath_source, const char* devpath_destination);

int fsd_do_check(const fs_data_t* fs, const char* optional_parameters,
		 bool repair, const char* output_file);
int fsd_do_update_gfs_config();
int fsd_do_update_cman(const char* nodename);
int fsd_do_set_gfs_logs(const fs_data_t* fs, int number_of_logs, int* final_number);
int fsd_do_update_gfs_tuning(const fs_data_t* fs);

#endif /* H_FSCOMMANDS */

