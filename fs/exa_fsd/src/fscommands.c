/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>

#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_env.h"
#include "os/include/strlcpy.h"
#include "common/include/exa_names.h"
#include "common/include/exa_config.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"

#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "fs/include/exa_fsd.h"
#include "fs/include/exactrl-fs.h"

#include "fs/exa_fsd/src/exa_fsd.h"

#include "fs/exa_fsd/src/fscommands.h"

#define EXA_FSSCRIPT  "exa_fsscript"

#define FSD_ACTION_PREPARE "PREPARE"
#define FSD_ACTION_UNLOAD  "UNLOAD"
#define FSD_ACTION_POST_MOUNT "POST_MOUNT"
#define FSD_ACTION_PRE_UMOUNT "PRE_UMOUNT"

/* These are internal error code defined in the exa_fsscript */
#define ERR_INTERNAL                  1
#define ERR_MODULE                    2
#define ERR_EXECUTION                 3
#define ERR_SIZE                      4
#define ERR_TIMEOUT                   5
#define ERR_MKFS                      6

/** \brief execute an external programm
 * This function is a wrapper around a fork + exec facility. It is copied
 * from admind_system. The difference is in the invocation process : only 1
 * string argument, called with "system", not "execvp".
 *
 * \param[in] command  the command to run with all necessary arguments.
 *
 * \return -1 if error (command not executed) or the return exit value
 */
static int
fsd_system(const char * command)
{
  int retval;
  pid_t pid;
  int fd;

  int pipe_system[2];
  int system_return;

  if (!command || !command[0])
    return -EINVAL;

  retval = pipe(pipe_system);
  if (retval != 0)
      return errno;

  pid = fork();
  switch (pid) {
    case -1:
      retval = -errno;
      exalog_error("cannot fork error: %s", strerror(-retval));
      return retval;

    case 0: /* child */
      close(pipe_system[0]);
      retval = chdir(RUNNING_DIR);
      if (retval == -1)
      {
	retval = -errno;
	exalog_as(EXAMSG_ADMIND_ID);
	exalog_error("chdir(" RUNNING_DIR ") failed: %s",
		     strerror(retval));
	return retval;
      }
      setsid ();

      /* close std file descriptors */
      for (fd = getdtablesize() - 1; fd >= 0; fd--)
      {
	  if (fd != pipe_system[1])
	  {
	      close(fd);
	  }
      }

      fd = open ("/dev/null", O_RDWR);
      retval = dup(fd);
      if (retval == -1)
      {
	retval = -errno;
	exalog_as(EXAMSG_ADMIND_ID);
	exalog_error("dup() failed: %s", strerror(retval));
	return retval;
      }
      retval = dup(fd);
      if (retval == -1)
      {
	retval = -errno;
	exalog_as(EXAMSG_ADMIND_ID);
	exalog_error("dup() failed: %s", strerror(retval));
	return retval;
      }

      system_return = system(command);
      retval = write(pipe_system[1], &system_return, sizeof(system_return));
      if (retval == -1)
      {
        retval = -errno;
        exalog_as(EXAMSG_ADMIND_ID);
        exalog_error("write() failed: %s", strerror(retval));
        return retval;
      }
      close(pipe_system[1]);

      exit(0); /* No execution problem ! */

    default: /* parent */
    {
      close(pipe_system[1]);
      retval = read(pipe_system[0], &system_return, sizeof(system_return));
      close(pipe_system[0]);
      if (retval == -1)
      {
        retval = -errno;
        exalog_as(EXAMSG_ADMIND_ID);
        exalog_error("read() failed: %s", strerror(retval));
        return retval;
      }
      pid = waitpid(pid, &retval, 0);
    }
  }

  if (-1 == pid) /* waitpid failed */
    {
      retval = -errno;
      exalog_debug("Cannot exec '%s': "
	           "system failed. error = %d", command, retval);
      return retval;
    }

  EXA_ASSERT_VERBOSE((WIFEXITED (retval)),
                     "incoherent return from waitpid %d %d", pid, retval);

  exalog_debug("Command '%s' returned %i ", command, system_return);

  return system_return;
}

/**
 * \brief Handle the request "fsinfo": Check if the filesystem is mounted
 *
 * \param[in] devpath	Path of the device block
 *
 * \return EXA_FS_MOUNTED_RO if the path is mounted RO,
 *         EXA_FS_MOUNTED_RW if the path is mounted RW,
 *         EXA_FS_UNMOUNTED if unmounted.
 */
int
fsd_do_fsinfo(const char *devpath)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char cmd_rw[EXA_MAXSIZE_BUFFER + 1];

  EXA_ASSERT(devpath && strlen(devpath) > 0);

  os_snprintf(cmd, sizeof(cmd), "cat /proc/mounts | grep -q \"^%s \"", devpath);
  /* grep return 0 when found (mounted) */

  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  exalog_debug("fsd_do_fsinfo : mounted ? returned %i",WEXITSTATUS(ret));

  if (ret==0)
    {
      os_snprintf(cmd_rw, sizeof(cmd_rw), "cat /proc/mounts | grep \"^%s \" | grep -q -E '[ ,]ro[ ,]'",devpath);

      ret = fsd_system(cmd_rw);
      exalog_debug("fsd_do_fsinfo : cmd_rw returned %i",ret);
      if (ret == -1)
	return -EXA_ERR_EXEC_FILE;
      exalog_debug("fsd_do_fsinfo (%s) : RW ? returned %i",cmd_rw,WEXITSTATUS(ret));
      if (!WEXITSTATUS(ret))
	return EXA_FS_MOUNTED_RO;
      return EXA_FS_MOUNTED_RW;
    }

  return EXA_FS_UNMOUNTED;
}

/**
 * \brief Handle the request "mountpointinfo": Check if the path is mounted
 *
 * \param[in] mountpoint  Path to check (/mnt/gfs)
 *
 * \return 1 if the path is mounted, 0 if not or negative error code
 */
int
fsd_do_mountpointinfo(const char *mountpoint)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char* unique_path=NULL;  /* Warning : this one needs to be freed */

  EXA_ASSERT(mountpoint && strlen(mountpoint) > 0);

  /* Solve mountpoint to the real path */
  unique_path=realpath(mountpoint,NULL);
  if (!unique_path)
    {
      return 0;
    }

  os_snprintf(cmd, sizeof(cmd), "cat /proc/mounts | grep -q \" %s \"", unique_path);
  /* grep return 0 when not found (not mounted) */

  free(unique_path);
  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  /* return 1 if grep return 0 and vice versa */
  return 1-WEXITSTATUS(ret);
}

/**
 * \brief Handle the request "dfinfo": Run statfs on the given FS
 *
 * \param[in]  mountpoint  Path to check (/mnt/gfs)
 * \param[out] capa        Fed with values from "statfs".
 *
 * \return 0 if ok or negative error code (errno)
 */
/* int wait_bool = 1; hack before yaourt*/
int
fsd_do_dfinfo(const char *mountpoint, struct fsd_capa *capa)
{
  int ret;
  struct statfs buf;

  EXA_ASSERT(mountpoint && strlen(mountpoint) > 0);

  capa->size = -1;
  capa->used = -1;
  capa->free = -1;

  exalog_debug("dfinfo mountpoint=%s", mountpoint);

  ret = statfs(mountpoint, &buf);
  if (ret)
    return -errno;

  exalog_debug("dfinfo mountpoint=%s blocksize=%" PRIzd " size=%" PRIu64 " available=%" PRIu64 " free=%" PRIu64,
      mountpoint,
      buf.f_bsize,
      buf.f_blocks,
      buf.f_bavail,
      buf.f_bfree);

  /* capa in bytes */
  capa->size = buf.f_blocks;
  capa->used = buf.f_blocks - buf.f_bfree;
  capa->free = buf.f_bavail;

  capa->size *= (uint64_t)buf.f_bsize;
  capa->used *= (uint64_t)buf.f_bsize;
  capa->free *= (uint64_t)buf.f_bsize;

  return 0;
}

/**
 * \brief Get the /dev file entry to the data device hold by this filesystem.
 *
 * \param[in] fs     Filesystem definition (name, other specific parameters)
 *
 * \return a constant string to file system main device file.
 */
const char* get_fs_devpath(const fs_data_t *fs)
{
  return fs->devpath;
}


/**
 * \brief Handle the request "mount"
 *
 * \param[in] fs             Filesystem definition (name, other specific parameters)
 * \param[in] read_only      1 if mount should always be done RO, 0 otherwise.
 * \param[in] mount_remount  Bit field for mount/remount options.
 *
 * \return 0 on success or an error code.
 */
int
fsd_do_mount(const fs_data_t *fs, int read_only, int mount_remount)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char mountpoint_path[EXA_MAXSIZE_MOUNTPOINT+1];
  char* mount_option="";
  struct stat stat_mountpoint;
  int mount_state;
  int mount_point_used;

  exalog_debug("Called mount with parameters: read_only=%"
	       PRId32" mount_remount=%"PRId32" mountpoint '%s' device '%s' ",
	       read_only, mount_remount, fs->mountpoint, fs->devpath);

  /* Create the directory. It takes the umask. On failure, mount will fail.*/
  ret=mkdir(fs->mountpoint, 0777);

  /* Solve mountpoint to the real path */
  {
    char* unique_path=NULL; /* Warning : this one needs to be freed */
    unique_path=realpath(fs->mountpoint,NULL);
    if (!unique_path)
      {
	/* Mountpoint was not created */
	return -FS_ERR_IMPOSSIBLE_MOUNTPOINT;
      }
    strlcpy(mountpoint_path, unique_path, EXA_MAXSIZE_MOUNTPOINT+1);
    free(unique_path);
  }

  /* Check it is a directory, and that it is created */
  ret=stat(mountpoint_path, &stat_mountpoint);
  if (ret!=0)
    {
      return -FS_ERR_IMPOSSIBLE_MOUNTPOINT;
    }
  if (!S_ISDIR(stat_mountpoint.st_mode))
    {
      return -FS_ERR_INVALID_MOUNTPOINT;
    }

  mount_state = fsd_do_fsinfo(get_fs_devpath(fs));
  mount_point_used = fsd_do_mountpointinfo(fs->mountpoint);
  /* If not mounted, check it is not mounted by an other file system */
  if ( (mount_state == EXA_FS_UNMOUNTED) && (mount_point_used != 0) )
    {
      return -FS_ERR_MOUNTPOINT_USED;
    }
  /* Exit early if nothing has to be done */
  if ( ( (mount_state==EXA_FS_MOUNTED_RW) || (mount_state==EXA_FS_MOUNTED_RO) )
       && (! (mount_remount & FS_ALLOW_REMOUNT ) ) )
    {
      exalog_debug("Already mounted and not allowed to remount. Do nothing.");
      return EXA_SUCCESS;
    }
  if ( ( (mount_state==EXA_FS_UNMOUNTED) )
       && (! (mount_remount & FS_ALLOW_MOUNT ) ) )
    {
      exalog_debug("Not mounted and not allowed to mount. Do nothing.");
      return EXA_SUCCESS;
    }
  if (mount_state==EXA_FS_MOUNTED_RW)
    {
      /* already mounted RW */
      if (read_only == 1)
	{
	  mount_option="remount,ro";
	}
      else
	{
	  exalog_debug("Already in state RW, and need mount in RW. Do nothing.");
	  return EXA_SUCCESS;
	}
    }
  else if (mount_state==EXA_FS_MOUNTED_RO)
    {
      /* already mounted RO */
      if (read_only == 1)
	{
	  exalog_debug("Already in state RO, and volume is DOWN or RO. Do nothing.");
	  return EXA_SUCCESS;
	}
      else
	{
	  mount_option="remount,rw";
	}
    }
  else
    {
      /* unmounted */
      if (read_only == 1)
	{
	  mount_option="ro";
	}
      else
	{
	  mount_option="rw";
	}
    }

  /* For GFS, we must set the "locktable" option to the file system's
   * UUID value.
   * For Ext3, we enable file system's barriers by default.
   */
  if (strcmp(fs->fstype, FS_NAME_GFS) == 0)
    os_snprintf(cmd, sizeof(cmd),
             "mount -o %s%s%s,locktable=%s -t %s %s %s",
             fs->mount_option,
             strcmp(fs->mount_option, "") ? "," : "",
             mount_option, fs->clustered.gfs.uuid, fs->fstype,
             get_fs_devpath(fs), mountpoint_path);
  else if (strcmp(fs->fstype, FS_NAME_EXT3) == 0)
    os_snprintf(cmd, sizeof(cmd),
             "mount -o %s%s%s,barrier=1 -t %s %s %s",
             fs->mount_option,
             strcmp(fs->mount_option, "") ? "," : "",
             mount_option,
             fs->fstype,
             get_fs_devpath(fs), mountpoint_path);
  else if (strcmp(fs->fstype, FS_NAME_XFS) == 0)
    os_snprintf(cmd, sizeof(cmd),
             "mount -o %s%s%s -t %s %s %s",
             fs->mount_option,
             strcmp(fs->mount_option, "") ? "," : "",
             mount_option,
             fs->fstype,
             get_fs_devpath(fs), mountpoint_path);
  else
    EXA_ASSERT_VERBOSE(false, "Unknown file system type: %s", fs->fstype);

  ret = fsd_system(cmd);
  if (ret == -1)
    {
      return -EXA_ERR_EXEC_FILE;
    }
  if (WEXITSTATUS(ret))
    {
      exalog_error("Mount command : '%s' returned %d", cmd, WEXITSTATUS(ret));
      return -FS_ERR_MOUNT_ERROR;
    }

  /* GFS-specific : set read-ahead */
  if (!strcmp(fs->fstype, FS_NAME_GFS))
    {
      ret = fsd_do_update_gfs_tuning(fs);
      if (ret)
	return ret;
    }

  return EXA_SUCCESS;
}

/**
 * Handle the request "post_mount". Give a chance to the user to
 * run a specific command once the filesystem is mounted.
 *
 * \param[in] fs     Filesystem definition (name, other specific parameters)
 * \param[in] group_name Group name
 * \param[in] fs_name Filesystem name
 * \return 0 on success or an error code.
 */
int
fsd_do_post_mount(const fs_data_t *fs, const char *group_name,
		  const char *fs_name)
{
  char cmd[EXA_MAXSIZE_BUFFER + 1];

  exalog_debug("Called post_mount with parameters: mountpoint '%s' "
	       "group_name '%s' fs_name '%s'",
	       fs->mountpoint, group_name, fs_name);

  // exa_fsscript POST_MOUNT fsname mountpoint [log_level]
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s %s %s %s",
              exa_env_sbindir(), EXA_FSSCRIPT, FSD_ACTION_POST_MOUNT,
              fs->fstype, group_name, fs_name, fs->mountpoint);
  exalog_debug("fsd_do_post_mount, command='%s'",cmd);

  /* It's a user script in the end, we don't manage errors there */
  fsd_system(cmd);

  return EXA_SUCCESS;
}

/**
 * Handle the request "pre_umount". Give a chance to the user to
 * run a specific command before the filesystem is unmounted.
 *
 * \param[in] fs     Filesystem definition (name, other specific parameters)
 * \param[in] group_name Group name
 * \param[in] fs_name Filesystem name
 * \return 0 on success or an error code.
 */
int
fsd_do_pre_umount(const fs_data_t *fs, const char *group_name,
		  const char *fs_name)
{
  char cmd[EXA_MAXSIZE_BUFFER + 1];

  exalog_debug("Called preumount with parameters: mountpoint '%s' "
	       "group_name '%s' fs_name '%s'",
	       fs->mountpoint, group_name, fs_name);

  // exa_fsscript PRE_UMOUNT fsname mountpoint [log_level]
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s %s %s %s",
              exa_env_sbindir(), EXA_FSSCRIPT, FSD_ACTION_PRE_UMOUNT,
              fs->fstype, group_name, fs_name, fs->mountpoint);
  exalog_debug("fsd_do_pre_umount, command='%s'",cmd);

  /* It's a user script in the end, we don't manage errors there */
  fsd_system(cmd);

  return EXA_SUCCESS;
}

/**
 * \brief Handle the request "unmount"
 *
 * \param[in] fs     Filesystem definition (name, other specific parameters)
 *
 * \return 0 on success or an error code.
 */
int
fsd_do_umount(const fs_data_t* fs)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char* unique_path=NULL; /* Warning : this one needs to be freed */

  /* Unmount if and only if it is mounted */
  if (fsd_do_fsinfo(get_fs_devpath(fs)) != EXA_FS_UNMOUNTED)
    {
      /* Solve mountpoint to the real path */
      unique_path=realpath(fs->mountpoint,NULL);
      if (!unique_path)
	{
	  return -FS_ERR_UMOUNT_ERROR;
	}

      /* umount device */
      os_snprintf(cmd, sizeof(cmd), "umount %s", unique_path);
      free(unique_path);
      exalog_debug("fsd umount action command : %s ",cmd);
      ret = fsd_system(cmd);
      if (ret == -1)
	{
	return -EXA_ERR_EXEC_FILE;
	}
      if (ret != 0)
	{
	return -ret;
	}
      /* try to delete directory. On failure, this means the directory
	 contains data and we must keep it "as is". */
      os_snprintf(cmd, sizeof(cmd), "rmdir %s", fs->mountpoint);
      ret = fsd_system(cmd);
      exalog_debug("fsd rmdir action command %s  returned %u",cmd,ret);
      return EXA_SUCCESS;
    }
  else
    {
      /* Unmounting an unmounted device is not a failure */
      ret=EXA_SUCCESS;
    }
  return -WEXITSTATUS(ret);
}

/**
 * \brief Handle the request "prepare filesystem"
 *
 * \param[in] fs     Filesystem definition (name, other specific parameters)
 *
 * \return 0 on success or an error code.
 */
int fsd_do_prepare(const fs_data_t* fs)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char gfs_options[EXA_MAXSIZE_BUFFER + 1];

  EXA_ASSERT(fs);
  EXA_ASSERT(strlen(fs->fstype) > 0);

  exalog_debug("fsd_do_prepare %s",fs->fstype);

  memset(gfs_options, 0, EXA_MAXSIZE_BUFFER + 1);
  if (!strcmp(fs->fstype,FS_NAME_GFS))
    {
      os_snprintf(gfs_options, sizeof(gfs_options), "%s %s", fs->clustered.gfs.lock_protocol, hostname);
    }

  // exa_fsscript PREPARE gfs [lock_gulm|lock_dlm hostname log_level]
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s %s %s",
              exa_env_sbindir(), EXA_FSSCRIPT, FSD_ACTION_PREPARE,
              fs->fstype, gfs_options,
#ifdef DEBUG
	   "DEBUG"
#else
	   "NODEBUG"
#endif
	   );
  exalog_debug("fsd_do_prepare, command='%s'",cmd);

  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  switch (WEXITSTATUS(ret))
    {
    case ERR_INTERNAL:
      EXA_ASSERT_VERBOSE(false, "Internal error inside exa_fsscript");
      break;
    case ERR_MODULE:
      return -FS_ERR_LOAD_MODULE;
      break;
    case ERR_EXECUTION:
      return -FS_ERR_EXECUTION_ERROR;
    case ERR_SIZE:
      return -FS_ERR_MKFS_SIZE_ERROR;
    case ERR_TIMEOUT:
      return -FS_ERR_TIME_OUT;
      break;
    }

  if (strcmp(fs->fstype, FS_NAME_GFS) == 0)
  {
      if (strcmp(fs->clustered.gfs.lock_protocol, "lock_gulm") == 0)
          gfs_set_gulm_running(true);
  }

  return EXA_SUCCESS;
}

/**
 * \brief Handle the request "unload filesystem"
 *
 * \param[in] fs   filesystem definition
 *
 * \return 0 on success or an error code.
 */
int fsd_do_unload(const fs_data_t* fs)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];

  EXA_ASSERT(fs);
  EXA_ASSERT(strlen(fs->fstype) > 0);

  exalog_debug("fsd_do_unload, fs mountpoint '%s' type '%s'", fs->mountpoint, fs->fstype);

  if (strcmp(fs->fstype, FS_NAME_GFS) == 0)
      gfs_set_gulm_running(false);

  // exa_fsscript UNLOAD [gfs|gfs-6.1]
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s",
              exa_env_sbindir(), EXA_FSSCRIPT,FSD_ACTION_UNLOAD,
              fs->fstype);

  exalog_debug("fsd_do_unload, execute '%s'", cmd);
  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  return -WEXITSTATUS(ret);
}

/**
 * \brief Handle the request "creategfs"
 *
 * \param[in] fs   Filesystem definition (name, lock protocol)
 *
 * \return 0 on success or an error code.
 */
int fsd_do_creategfs(const fs_data_t* fs)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];

  /* exa_fsscript CREATE gfs nb-logs device lock_proto uuid rg_size */
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s %"PRIu64" %s %s %s %"PRIu64,
              exa_env_sbindir(), EXA_FSSCRIPT, "CREATE", FS_NAME_GFS,
              fs->clustered.gfs.nb_logs,
              get_fs_devpath(fs),
              fs->clustered.gfs.lock_protocol,
              fs->clustered.gfs.uuid,
              fs->clustered.gfs.rg_size);

  exalog_debug("fsd_do_creategfs issues command : %s", cmd);

  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;
  switch (WEXITSTATUS(ret))
    {
    case ERR_INTERNAL:
      EXA_ASSERT_VERBOSE(false, "Internal error inside exa_fsscript");
      break;
    case ERR_EXECUTION:
      return -FS_ERR_EXECUTION_ERROR;
    case ERR_SIZE:
      return -FS_ERR_MKFS_SIZE_ERROR;
    case ERR_MKFS:
      return -FS_ERR_MKFS_ERROR;
      break;
    }

  return EXA_SUCCESS;
}

/**
 * \brief Handle the request "createlocal"
 *
 * \param[in] fstype      Type of filesystem (ext3, ...)
 * \param[in] devpath     Path of the GFS volume block device
 *
 * \return 0 on success or an error code.
 */
int fsd_do_createlocal(const char* fstype, const char* devpath)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];

  EXA_ASSERT(fstype && strlen(fstype) > 0);
  EXA_ASSERT(devpath && strlen(devpath) > 0);

  // exa_fsscript CREATE ext3...
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s %s",
              exa_env_sbindir(), EXA_FSSCRIPT, "CREATE",
              fstype, devpath);

  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;
  switch (WEXITSTATUS(ret))
    {
    case ERR_INTERNAL:
      EXA_ASSERT_VERBOSE(false, "Internal error inside exa_fsscript");
      break;
    case ERR_EXECUTION:
      return -FS_ERR_EXECUTION_ERROR;
    case ERR_MKFS:
      return -FS_ERR_MKFS_ERROR;
    }
  return EXA_SUCCESS;
}

/**
 * \brief Handle the request "resize"
 *
 * \param[in] fstype      Type of filesystem (gfs, ...)
 * \param[in] devpath     Device of the filesystem to resize
 * \param[in] mountpoint  Mountpoint of the filesystem to resize when it needs be mounted
 * \param[in] prepare     Action to perform.
 * \param[in] sizeKB      The new size in KB or 0 to use the size of the device
 *
 * \return 0 on success or an error code.
 */
int fsd_do_resize(const char* fstype, const char* devpath,
		  const char* mountpoint,
		  const int prepare, const uint64_t sizeKB)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char* action;

  EXA_ASSERT(fstype && strlen(fstype) > 0);
  EXA_ASSERT(devpath && strlen(devpath) > 0);

  exalog_debug("fsd_do_resize(%s, %s, %s, %d, %"PRId64,
      fstype, devpath, mountpoint, prepare, sizeKB);

  switch (prepare)
    {
    case FSRESIZE_PREPARE: action="PREPARERESIZE"; break;
    case FSRESIZE_RESIZE: action="RESIZE"; break;
    case FSRESIZE_FINALIZE: action="FINALIZERESIZE"; break;
    default: EXA_ASSERT_VERBOSE(false, "Unknown action for resize.");
    }
  // exa_fsscript RECOVERDOWN gfs master nodedown
  os_snprintf(cmd, sizeof(cmd), "%s%s %s %s %s %s %"PRIu64,
              exa_env_sbindir(), EXA_FSSCRIPT, action,
              fstype, devpath, mountpoint, sizeKB);

  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  switch (WEXITSTATUS(ret))
    {
    case ERR_SIZE:
      return -FS_ERR_RESIZE_NOT_ENOUGH_SPACE;
      break;
    default:
      return -WEXITSTATUS(ret);
    }
}

/**
 * \brief Handle the request "check"
 *
 * \param[in] fs                       Pointer to the FS data structure.
 * \param[in] optional_parameters      Optional parameters to give to fsck.
 * \param[in] repair                   Repair or just check ?
 * \param[in] output_file              In which file to store result.
 *
 * \return 0 on success or an error code.
 */
int fsd_do_check(const fs_data_t* fs, const char* optional_parameters,
		 bool repair, const char* output_file)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char fstype_specific_parameters[EXA_MAXSIZE_BUFFER + 1];
  int error_val;

  exalog_debug("fsd_do_check(%s, %s, %s, %s)", fs->devpath, fs->fstype, optional_parameters,output_file);

  fstype_specific_parameters[0]='\0';
  if (!strcmp(fs->fstype,FS_NAME_GFS))
    {
      os_snprintf(fstype_specific_parameters, sizeof(fstype_specific_parameters),
	       "%s" , fs->clustered.gfs.lock_protocol);
    }

  os_snprintf(cmd, sizeof(cmd), "%s%s CHECK %s %s '%s' '%s' '%s' '%s'",
              exa_env_sbindir(), EXA_FSSCRIPT,
              fs->fstype, fs->devpath, optional_parameters, output_file,
              repair ? "TRUE" : "FALSE", fstype_specific_parameters);

  exalog_debug("Run command for check : '%s'", cmd);
  ret = fsd_system(cmd);
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  error_val = EXA_SUCCESS;

  /* Segmentation fault, and so on... */
  if (WIFSIGNALED(ret))
    error_val= -FS_ERR_FSCK_ERROR;
  else
    {
      /* Normal exit status that fsck can give */
      switch (WEXITSTATUS(ret))
	{
	case 1:
	case 2:
	  error_val = -FS_ERR_CHECK_ERRORS_CORRECTED;
	  break;
	case 4:
	  error_val = -FS_ERR_CHECK_ERRORS;
	  break;
	case 8 :
	  error_val = -FS_ERR_CHECK_OPERATIONAL_ERROR;
	  break;
	case 16 :
	case 32 :
	case 128 :
	  error_val = -FS_ERR_CHECK_USAGE_ERROR;
	  break;
	}
    }
  return error_val;
}

/**
 * \brief Handle the request "updategfsconfig"
 *
 * \return 0 on success or an error code.
 */
int fsd_do_update_gfs_config(void)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  exalog_debug("fsd_do_update_gfs_config");

  strcpy(cmd, "ccs_tool update /etc/cluster/cluster.conf");

  ret = fsd_system(cmd);
  exalog_debug("Run command for fsd_do_update_gfs_config : '%s', returned %i", cmd, WEXITSTATUS(ret));
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  return -WEXITSTATUS(ret);
}

/**
 * \brief Handle the request "updatecman"
 *
 * \param[in] nodename   node that left the cluster.
 *
 * \return 0 on success or an error code.
 */
int fsd_do_update_cman(const char* nodename)
{
  int ret;
  char cmd[EXA_MAXSIZE_BUFFER + 1];

  sprintf(cmd, "cman_tool kill -n %s", nodename);
  ret = fsd_system(cmd);
  exalog_debug("Run command for fsd_do_update_cman (%s) : '%s', returned %i",
	       nodename, cmd, WEXITSTATUS(ret));
  if (ret == -1)
    return -EXA_ERR_EXEC_FILE;

  return -WEXITSTATUS(ret);
}

/**
 * \brief Handle the request "FSREQUEST_SET_LOGS"
 *
 * \param[in]  fs                FS whose number of logs must be modified.
 * \param[in]  number_of_logs    how many logs to obtain
 * \param[out] final_number      how many logs obtained
 *
 * \return the number of logs on success or a negative error code.
 */
int fsd_do_set_gfs_logs(const fs_data_t* fs, int number_of_logs, int* final_number)
{
  int ret, number_read;
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char number_logs_str[EXA_MAXSIZE_BUFFER + 1];
  const char* output_file = "/tmp/exa_fstune_result";
  FILE* result_file;

  *final_number = 0;

  os_snprintf(cmd, sizeof(cmd), "%s%s SET_LOGS '%s' '%s' '%s' '%i' '%s'",
              exa_env_sbindir(), EXA_FSSCRIPT,
              fs->fstype, fs->mountpoint, fs->devpath,
              number_of_logs, output_file);

  exalog_debug("Run command for fsd_do_set_gfs_journals : '%s'", cmd);
  ret = fsd_system(cmd);
  exalog_debug("Command returned '%i'", ret);
  if (ret == -1)
      return -EXA_ERR_EXEC_FILE;
  if (WEXITSTATUS(ret) != 0)
      return -WEXITSTATUS(ret);

  result_file = fopen(output_file, "r");
  if (result_file == NULL)
      return -EXA_ERR_OPEN_FILE;

  memset(number_logs_str, 0, EXA_MAXSIZE_BUFFER + 1);
  number_read = fread(number_logs_str, 1, EXA_MAXSIZE_BUFFER, result_file);
  if (number_read <= 0)
  {
      fclose(result_file);
      return -EXA_ERR_READ_FILE;
  }

  fclose(result_file);

  if (to_int(number_logs_str, final_number) != EXA_SUCCESS)
      return -FS_ERR_EXECUTION_ERROR;

  exalog_debug("New number of logs or error code '%i'", *final_number);
  if (*final_number <= 0)
      return -FS_ERR_EXECUTION_ERROR;

  return EXA_SUCCESS;
}

/**
 * \brief Handle the request update gfs tuning : RA and fuzzy_statfs
 *
 * \param[in]  fs   FS whose tuning must be set/updated
 *
 * \return 0 on success or an error code.
 */
int fsd_do_update_gfs_tuning(const fs_data_t* fs)
{
  char cmd[EXA_MAXSIZE_BUFFER + 1];
  char unique_path[PATH_MAX];
  char* result_path;
  int ret_sfs_tune;
  char * statfs_fast_value;
  /* Get real mount point */
  result_path = realpath(fs->mountpoint, unique_path);
  if (!result_path)
    {
      /* Mountpoint was not created i.e not mounted */
      return 0;
    }

  /* Mounted ? */
  if (!fsd_do_mountpointinfo(fs->mountpoint))
    {
      return 0; /* Valid, not mounted here */
    }

  os_snprintf(cmd, sizeof(cmd),
	   "sfs_tool settune %s seq_readahead %"PRIu64,
	   unique_path,
	   fs->clustered.gfs.read_ahead / 4);
  ret_sfs_tune = fsd_system(cmd);
  if (ret_sfs_tune == -1)
    {
      return -EXA_ERR_EXEC_FILE;
    }
  if (WEXITSTATUS(ret_sfs_tune))
    {
      exalog_error("set sfs fuzzy_statfs: '%s' returned an error : %d", cmd,
		   WEXITSTATUS(ret_sfs_tune));
      return -WEXITSTATUS(ret_sfs_tune);
    }
  statfs_fast_value = fs->clustered.gfs.fuzzy_statfs ? "1" : "0";
  os_snprintf(cmd, sizeof(cmd),
	   "sfs_tool settune %s statfs_fast %s", unique_path, statfs_fast_value);
  ret_sfs_tune = fsd_system(cmd);
  if (ret_sfs_tune == -1)
    {
      return -EXA_ERR_EXEC_FILE;
    }
  if (WEXITSTATUS(ret_sfs_tune))
    {
      exalog_error("set sfs statfs_fast: '%s' returned an error : %d", cmd,
		   WEXITSTATUS(ret_sfs_tune));
      return -WEXITSTATUS(ret_sfs_tune);
    }

  /* Set demote_secs */
  os_snprintf(cmd, sizeof(cmd), "sfs_tool settune %s demote_secs %u",
           unique_path, fs->clustered.gfs.demote_secs);
  ret_sfs_tune = fsd_system(cmd);
  if (ret_sfs_tune == -1)
    return -EXA_ERR_EXEC_FILE;
  if (WEXITSTATUS(ret_sfs_tune))
  {
    exalog_error("set sfs demote_secs: '%s' returned an error : %d", cmd,
                 WEXITSTATUS(ret_sfs_tune));
    return -WEXITSTATUS(ret_sfs_tune);
  }

  /* Set glock_purge */
  os_snprintf(cmd, sizeof(cmd), "sfs_tool settune %s glock_purge %u",
           unique_path, fs->clustered.gfs.glock_purge);
  ret_sfs_tune = fsd_system(cmd);
  if (ret_sfs_tune == -1)
    return -EXA_ERR_EXEC_FILE;
  if (WEXITSTATUS(ret_sfs_tune))
  {
    exalog_error("set sfs glock_purge: '%s' returned an error : %d", cmd,
                 WEXITSTATUS(ret_sfs_tune));
    return -WEXITSTATUS(ret_sfs_tune);
  }

  return 0;
}
