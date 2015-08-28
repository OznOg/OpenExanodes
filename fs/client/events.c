/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** \file
 * \brief Filesystem daemon client API.
 */

#include <string.h>

#include "common/include/daemon_api_client.h"
#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"
#include "fs/include/exactrl-fs.h"

/* === Internal functions ============================================ */

/**
 * \brief Internal function. Prepare/mount/umount/unload a filesystem.
 *
 * \param[in] mh	     Examsg handle.
 * \param[in] fs             Structure containing filesystem definition
 * \param[in] read_only      RO ?
 *
 * \return 1 if the block device is mounted, 0 if not or negative
 * error code
 */
static int
_fsd_startstopaction(ExamsgHandle mh, FSRequestType action,
		     const fs_data_t* fs, int read_only, int mount_remount)
{
  FSRequest req;
  FSAnswer answer;
  int ret;

  req.requesttype = action;
  memcpy(&req.argument.startstop.fs, fs, sizeof(req.argument.startstop.fs));

  req.argument.startstop.read_only = read_only;
  req.argument.startstop.mount_remount = mount_remount;

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;

  return answer.ack;
}

/* === Client API ==================================================== */

/**
 * \brief Check if the filesystem is mounted on the node where this
 * function is called.
 *
 * \param[in] mh	Examsg handle.
 * \param[in] devpath	Path of the block device (/dev/exa/group/volume)
 *
 * \return 1 if the block device is mounted, 0 if not or negative
 * error code
 */
int
fsd_is_fs_mounted(ExamsgHandle mh, const char* devpath)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  EXA_ASSERT(devpath);

  req.requesttype = FSREQUEST_FSINFO;
  strlcpy(req.argument.fsinfo.devpath, devpath, sizeof(req.argument.fsinfo.devpath));

  ret = admwrk_daemon_query_nointr(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
				   &req, sizeof(req),
				   &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Check if the path is currently used as a mountpoint
 *
 * \param[in] mh          Examsg handle.
 * \param[in] mountpoint  Path of the mountpoint (/mnt/gfs)
 *
 * \return 1 if the path is mounted, 0 if not or negative
 * error code
 */
int
fsd_is_mountpoint_used(ExamsgHandle mh, const char* mountpoint)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  EXA_ASSERT(mountpoint);

  req.requesttype = FSREQUEST_MOUNTPOINTINFO;
  strlcpy(req.argument.mountpointinfo.mountpoint, mountpoint, sizeof(req.argument.mountpointinfo.mountpoint));

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Get capacity of the filesystem from the command "df".
 *
 * Set capa to -1 on error.
 *
 * If the given mountpoint is not a mountpoint, the behaviour is
 * the same as the command df: we give the capacity of the
 * filesystem that contains the given directory.
 *
 *
 * \param[in] mh          Examsg handle.
 * \param[in] mountpoint  Path of the mountpoint (/mnt/gfs)
 * \param[out] capa       Capacity (size, used, free)
 *
 * \return 0 if success or negative error code
 */
int fsd_df(ExamsgHandle mh, const char* mountpoint, struct fsd_capa *capa)
{
  int ret;
  FSRequest req;
  FSAnswer answer;

  EXA_ASSERT(mountpoint);
  EXA_ASSERT(capa);

  req.requesttype = FSREQUEST_DF_INFO;
  strlcpy(req.argument.dfinfo.mountpoint, mountpoint, sizeof(req.argument.dfinfo.mountpoint));

  ret = admwrk_daemon_query_nointr(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
				   &req, sizeof(req),
				   &answer, sizeof(answer));
  if (ret != 0)
    return ret;

  *capa = answer.capa;

  return answer.ack;
}

/**
 * Prepare a local filesystem. Must be called on all nodes.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fs          Structure containing filesystem definition
 *
 * \return 0 on success or an error code.
 */
int
fsd_prepare(ExamsgHandle mh, const fs_data_t* fs)
{
  return _fsd_startstopaction(mh, FSREQUEST_PREPARE, fs, 0, 0);
}

/**
 * \brief Mount a local filesystem. Must be called only on the one node,
 * where the filesystem must be mounted.
 *
 * \param[in] mh	      Examsg handle.
 * \param[in] fs              Structure containing filesystem definition
 * \param[in] read_only       Do this RO or RW ?
 * \param[in] mount_remount   Flags to allow to mount a volume DOWN if already mounted.
 * \param[in] group_name      Group name
 * \param[in] fs_name         File system name
 *
 * \return 0 on success or an error code.
 */
int
fsd_mount(ExamsgHandle mh, const fs_data_t* fs,
	  int read_only, int mount_remount,
	  const char *group_name,
	  const char *fs_name)
{
  int ret;

  EXA_ASSERT(group_name);
  EXA_ASSERT(fs_name);

  ret =  _fsd_startstopaction(mh, FSREQUEST_MOUNT, fs, read_only,
			      mount_remount);

  {
    FSRequest req;
    FSAnswer answer;

    req.requesttype = FSREQUEST_POSTMOUNT;
    memcpy(&req.argument.startstop.fs, fs, sizeof(req.argument.startstop.fs));
    memcpy(&req.argument.startstop.fs_name,
	   fs_name,
	   sizeof(req.argument.startstop.fs_name));

    memcpy(&req.argument.startstop.group_name,
	   group_name,
	   sizeof(req.argument.startstop.group_name));

    /* We don't care of the error, mount action prevails */
    admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
			&req, sizeof(req),
			&answer, sizeof(answer));
  }

  return ret;
}

/**
 * \brief Umount a filesystem. Must be called on every node
 * where the filesystem is mounted.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fs          Structure containing filesystem definition
 * \param[in] group_name  Group name
 * \param[in] fs_name     File system name
 *
 * \return 0 on success or an error code.
 */
int
fsd_umount(ExamsgHandle mh, const fs_data_t* fs,
	   const char *group_name,
	   const char *fs_name)
{
  {
    FSRequest req;
    FSAnswer answer;

    req.requesttype = FSREQUEST_PREUMOUNT;
    memcpy(&req.argument.startstop.fs, fs, sizeof(req.argument.startstop.fs));
    memcpy(&req.argument.startstop.fs_name,
	   fs_name,
	   sizeof(req.argument.startstop.fs_name));

    memcpy(&req.argument.startstop.group_name,
	   group_name,
	   sizeof(req.argument.startstop.group_name));

    /* We don't care of the error, mount action prevails */
    admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
			&req, sizeof(req),
			&answer, sizeof(answer));
  }

  return _fsd_startstopaction(mh, FSREQUEST_UMOUNT, fs, 0, 0);
}

/**
 * \brief Unload daemons & modules associated to afilesystem. Must be called
 * on all nodes.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fs          Structure containing filesystem definition
 *
 * \return 0 on success or an error code.
 */
int
fsd_unload(ExamsgHandle mh, const fs_data_t* fs)
{
  return _fsd_startstopaction(mh, FSREQUEST_UNLOAD, fs, 0, 0);
}

/**
 * \brief Create a new local filesystem. Must be called on only one node.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fstype      Type of filesystem (ext3, ...)
 * \param[in] devpath	  Path of the block device (/dev/exa/group/volume)
 *
 * \return 0 on success or an error code.
 */
int fsd_fs_create_local(ExamsgHandle mh, const char* fstype, const char* devpath)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  EXA_ASSERT(fstype);
  EXA_ASSERT(devpath);

  req.requesttype = FSREQUEST_CREATELOCAL;
  strlcpy(req.argument.createlocal.devpath, devpath, sizeof(req.argument.createlocal.devpath));
  strlcpy(req.argument.createlocal.fstype, fstype, sizeof(req.argument.createlocal.fstype));

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Create a new GFS filesystem. Must be called on only one node.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fs          Structure containing filesystem definition
 *
 * \return 0 on success or an error code.
 */
int fsd_fs_create_gfs(ExamsgHandle mh, const fs_data_t* fs)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  EXA_ASSERT(fs);

  req.requesttype = FSREQUEST_CREATEGFS;
  memcpy(&req.argument.creategfs.fs, fs, sizeof(req.argument.creategfs.fs));

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Prepare the resize/finalize a filesystem (fsck). Must be called on only one node.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fstype      Type of filesystem (ext3, gfs, ...)
 * \param[in] devpath     Path of the block device
 * \param[in] mountpoint  Where the filesystem is mounted (or should be, if need be).
 * \param[in] sizeKB      New size in KB or 0 to use the size of the device
 * \param[in] action      Action to perform (resize, prepare, finalize)
 *
 * \return 0 on success or an error code.
 */
static int fsd_fs_resize_action(ExamsgHandle mh,
				const char* fstype,
				const char* devpath,
				const char* mountpoint,
				const uint64_t sizeKB, int action)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  EXA_ASSERT(fstype);
  EXA_ASSERT(devpath);

  req.requesttype = FSREQUEST_RESIZE;
  strlcpy(req.argument.resize.fstype, fstype, sizeof(req.argument.resize.fstype));
  strlcpy(req.argument.resize.mountpoint, mountpoint, sizeof(req.argument.resize.mountpoint));
  strlcpy(req.argument.resize.devpath, devpath, sizeof(req.argument.resize.devpath));
  req.argument.resize.sizeKB = sizeKB;
  req.argument.resize.action = action;

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Prepare the resize a filesystem (fsck). Must be called on only one node.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fstype      Type of filesystem (ext3, gfs, ...)
 * \param[in] devpath     Path of the block device
 * \return 0 on success or an error code.
 */
int fsd_prepare_resize(ExamsgHandle mh, const char* fstype, const char* devpath)
{
  return fsd_fs_resize_action(mh, fstype, devpath, "", 0, FSRESIZE_PREPARE);
}

/**
 * \brief Finalize the resize a filesystem (journal on ext3). Must be called on only one node.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fstype      Type of filesystem (ext3, gfs, ...)
 * \param[in] devpath     Path of the block device
 *
 * \return 0 on success or an error code.
 */
int fsd_finalize_resize(ExamsgHandle mh, const char* fstype, const char* devpath)
{
  return fsd_fs_resize_action(mh, fstype, devpath, "", 0, FSRESIZE_FINALIZE);
}

/**
 * \brief Resize a filesystem. Must be called on only one node.
 *
 * \param[in] mh	  Examsg handle.
 * \param[in] fstype      Type of filesystem (ext3, gfs, ...)
 * \param[in] devpath     Path of the block device
 * \param[in] mountpoint  Where the filesystem is mounted (or should be, if need be).
 * \param[in] sizeKB      New size in KB or 0 to use the size of the device
 *
 * \return 0 on success or an error code.
 */
int fsd_resize(ExamsgHandle mh, const char* fstype, const char* devpath, const char* mountpoint,
	       const uint64_t sizeKB)
{
  return fsd_fs_resize_action(mh, fstype, devpath, mountpoint, sizeKB, FSRESIZE_RESIZE);
}

/**
 * \brief Check a filesystem. Must be called on only one node.
 *
 * \param[in] mh 	        Examsg handle.
 * \param[in] fs                Pointer to the FS data structure.
 * \param[in] optional_parameters  "-F -..."
 * \param[in] repair              Repair or just check ?
 * \param[in] output_file       In which file to store result.
 *
 * \return 0 on success or an error code.
 */
int fsd_check(ExamsgHandle mh, const fs_data_t* fs,
	      const char* optional_parameters, bool repair,
	      const char* output_file)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  EXA_ASSERT(fs);
  EXA_ASSERT(optional_parameters);
  EXA_ASSERT(output_file);

  req.requesttype = FSREQUEST_CHECK;
  memcpy(&req.argument.check.fs, fs, sizeof(req.argument.check.fs));
  strlcpy(req.argument.check.optional_parameters, optional_parameters,
	  sizeof(req.argument.check.optional_parameters));
  strlcpy(req.argument.check.output_file, output_file,
	  sizeof(req.argument.check.output_file));
  req.argument.check.repair = repair;

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Update the config file of GFS.
 *
 * \param[in] mh 	        Examsg handle.
 * \return 0 on success or an error code.
 */
int fsd_gfs_update_config(ExamsgHandle mh)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  req.requesttype = FSREQUEST_UPDATE_GFS_CONFIG;

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Update the state of CMAN.
 *
 * \param[in] mh 	        Examsg handle.
 * \param[in] hostname 	        Node to KILL !
 * \return 0 on success or an error code.
 */
int fsd_cman_update(ExamsgHandle mh, const char* hostname)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  req.requesttype = FSREQUEST_UPDATE_CMAN;
  strlcpy(req.argument.updatecman.nodename, hostname,
	  sizeof(req.argument.updatecman.nodename));

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}

/**
 * \brief Increase the number of logs in GFS
 *
 * \param[in] mh 	        Examsg handle.
 * \param[in] fs 	        FS to increase
 * \param[in] number_of_logs	New number of logs
 * \return 0 a negative error code,
 *           and the new number of logs if positive.
 */
int fsd_set_gfs_logs(ExamsgHandle mh, const fs_data_t* fs, int number_of_logs)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  req.requesttype = FSREQUEST_SET_LOGS;
  req.argument.setlogs.number_of_logs = number_of_logs;
  memcpy(&req.argument.setlogs.fs, fs, sizeof(req.argument.setlogs.fs));

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  if (answer.ack != EXA_SUCCESS)
    return answer.ack;
  return answer.number_of_logs;
}


/**
 * \brief Update the tuning values for a SFS, if it's mounted.
 *
 * \param[in] mh 	        Examsg handle.
 * \param[in] fs 	        fs to change
 * \return 0 on success or an error code.
 */
int fsd_update_gfs_tuning(ExamsgHandle mh, const fs_data_t* fs)
{
  FSRequest req;
  int ret;
  FSAnswer answer;

  req.requesttype = FSREQUEST_UPDATE_GFS_TUNING;
  memcpy(&req.argument.updategfstuning.fs, fs,
	 sizeof(req.argument.updategfstuning.fs));

  ret = admwrk_daemon_query(mh, EXAMSG_FSD_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
  if (ret != 0)
    return ret;
  return answer.ack;
}
