/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** \file
 * \brief Test filesystem daemon routines
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/shm.h>

#include "common/include/daemon_api_client.h"
#include "common/include/exa_error.h"
#include "common/include/exa_conversion.h"

#include "examsg/include/examsg.h"

#include "fs/include/exa_fsd.h"
#include "fs/include/exactrl-fs.h"

#include "fs/include/fs_data.h"

#include "os/include/os_file.h"
#include "os/include/os_string.h"

/* Copy/Paste from type_gfs.c :
   Structure describing the shared memory segment used to talk to Gulm */
typedef struct fs_membership_shm_info_t
{
    uint64_t revision;
    uint64_t gulm_master;
    struct nodeinfo
    {
        uint64_t node_state;
        uint64_t state_changed;
        char hostname[EXA_MAXSIZE_HOSTNAME + 1];
    } nodes_info[EXA_MAX_NODES_NUMBER];
} fs_membership_shm_info_t;

static fs_membership_shm_info_t *membership_shm_info = NULL;
static ExamsgHandle mh;
static char *self;

void read_shm(void)
{
  /* Copy/paste from Gulm */
  /* Initialize the shared memory segment with appropriate values */
  int shmid, node_index;
  key_t fs_shm_key;

  printf("Reading info about memory segment shared with Gulm:\n");

  fs_shm_key = ftok("/var/cache/exanodes/fs_shm_key", 0);
  if (fs_shm_key == -1)
  {
      fprintf(stderr, "Cannot initialize shared memory key\n");
      return;
  }

  /* Try to attach shm, if it exists. */
  shmid = shmget(fs_shm_key , sizeof(fs_membership_shm_info_t), IPC_EXCL | 0600);
  if (shmid == -1)
  {
      /* If it doesn't exist, that's an unrecoverable error */
      fprintf(stderr, "Shared memory segment doesn't exist. How about Exanodes?\n");
      return;
  }

  membership_shm_info = (fs_membership_shm_info_t *)shmat(shmid, NULL, 0);
  if (membership_shm_info == (fs_membership_shm_info_t *)-1)
  {
      fprintf(stderr, "Error trying to attach shared memory segment\n");
      return;
  }

  printf("Revision %"PRIu64"\n", membership_shm_info->revision);
  printf("Master   %"PRIu64"\n", membership_shm_info->gulm_master);
  for (node_index = 0; node_index != EXA_MAX_NODES_NUMBER; node_index++)
  {
      if (strcmp("", membership_shm_info->nodes_info[node_index].hostname) != 0)
      {
	  printf("Index %i node_state %"PRIu64" state_changed %"PRIu64" hostname '%s'\n",
		 node_index,
		 membership_shm_info->nodes_info[node_index].node_state,
		 membership_shm_info->nodes_info[node_index].state_changed,
		 membership_shm_info->nodes_info[node_index].hostname);
      }
  }
}

void usage(void)
{
  fprintf(stderr, "usage: %s <action> [arg]\n", self);
  fprintf(stderr, "actions: \n");
  fprintf(stderr, "    is_fs_mounted <devpath>\n");
  fprintf(stderr, "    is_mountpoint_used <mountpoint>\n");
  fprintf(stderr, "    prepare_gfs <lock_protocol>\n");
  fprintf(stderr, "    mount <fstype> <mountpoint> <devpath> <group_name> <fs_name>\n");
  fprintf(stderr, "    umount <mountpoint> <devpath> <group_name> <fs_name>\n");
  fprintf(stderr, "    unload <fstype>\n");
  fprintf(stderr, "    create_local <fstype> <devpath>\n");
  fprintf(stderr, "    create_gfs <devpath> <lock_proto> <size> <uuid> <nb_logs>\n");
  fprintf(stderr, "    dfinfo <mountpoint>\n");
  fprintf(stderr, "    resize <fstype> <devpath> <mountpoint> 0|<newsizeKB>\n");
  fprintf(stderr, "    prepare_resize <fstype> <devpath>\n");
  fprintf(stderr, "    read_shm\n");
  fprintf(stderr, "    add_logs <devpath> <nb_logs>\n");
}

int main(int argc, char *argv[])
{
  bool static_init_ok = false;
  const char *action;
  int err = 0;

  self = os_program_name(argv[0]);

  if (argc <= 1)
  {
      usage();
      err = 1;
      goto done;
  }

  err = examsg_static_init(EXAMSG_STATIC_GET);
  if (err != 0)
  {
      fprintf(stderr, "examsg_static_init failed: %s (%d)\n", exa_error_msg(-err), err);
      goto done;
  }
  static_init_ok = true;

  mh = examsgInit(EXAMSG_TEST_ID);
  if (!mh)
  {
      fprintf(stderr, "examsgInit failed\n");
      goto done;
  }

  err = examsgAddMbox(mh, examsgOwner(mh), 3, EXAMSG_MSG_MAX);
  if (err != 0)
  {
      fprintf(stderr, "examsgAddMbox failed: %s (%d)\n", exa_error_msg(-err), err);
      goto done;
  }

  action = argv[1];
  if (strcmp(action, "is_fs_mounted") == 0)
  {
      int m;

      if (argc != 3)
      {
          usage();
          goto done;
      }

      m = fsd_is_fs_mounted(mh, argv[2]);
      if (m < 0)
          err = m;
  }
  else if (strcmp(action, "is_mountpoint_used") == 0)
  {
      int u;

      if (argc != 3)
      {
          usage();
          goto done;
      }

      u = fsd_is_mountpoint_used(mh, argv[2]);
      if (u < 0)
          err = u;
  }
  else if (strcmp(action, "prepare_gfs") == 0)
  {
      fs_data_t fs;
      size_t sz;

      if (argc != 3)
      {
          usage();
          goto done;
      }

      COMPILE_TIME_ASSERT(sizeof("sfs") <= sizeof(fs.fstype));
      os_strlcpy(fs.fstype, "sfs", sizeof(fs.fstype));

      sz = os_strlcpy(fs.clustered.gfs.lock_protocol, argv[2],
                      sizeof(fs.clustered.gfs.lock_protocol));
      if (sz >= sizeof(fs.clustered.gfs.lock_protocol))
      {
          fprintf(stderr, "Invalid lock protocol: '%s' (too long)\n", argv[2]);
          goto done;
      }

      err = fsd_prepare(mh, &fs);
  }
  else if (strcmp(action, "mount") == 0)
  {
      fs_data_t fs;
      size_t sz;

      if (argc != 7)
      {
          usage();
          goto done;
      }

      sz = os_strlcpy(fs.fstype, argv[2], sizeof(fs.fstype));
      if (sz >= sizeof(fs.fstype))
      {
          fprintf(stderr, "Invalid fs type: '%s' (too long)\n", argv[2]);
          goto done;
      }

      sz = os_strlcpy(fs.mountpoint, argv[3], sizeof(fs.mountpoint));
      if (sz >= sizeof(fs.mountpoint))
      {
          fprintf(stderr, "Invalid mountpoint: '%s' (too long)\n", argv[3]);
          goto done;
      }

      sz = os_strlcpy(fs.devpath, argv[4], sizeof(fs.devpath));
      if (sz >= sizeof(fs.devpath))
      {
          fprintf(stderr, "Invalid dev path: '%s' (too long)\n", argv[4]);
          goto done;
      }

      err = fsd_mount(mh, &fs, 1 , 0, argv[5], argv[6]);
  }
  else if (strcmp(action, "umount") == 0)
  {
      fs_data_t fs;
      size_t sz;

      if (argc != 6)
      {
          usage();
          goto done;
      }

      sz = os_strlcpy(fs.mountpoint, argv[2], sizeof(fs.mountpoint));
      if (sz >= sizeof(fs.mountpoint))
      {
          fprintf(stderr, "Invalid mountpoint: '%s' (too long)\n", argv[2]);
          goto done;
      }

      sz = os_strlcpy(fs.devpath, argv[3], sizeof(fs.devpath));
      if (sz >= sizeof(fs.devpath))
      {
          fprintf(stderr, "Invalid dev path: '%s' (too long)\n", argv[3]);
          goto done;
      }

      err = fsd_umount(mh, &fs, argv[5], argv[6]);
  }
  else if (strcmp(action, "unload") == 0)
  {
      fs_data_t fs;
      size_t sz;

      if (argc != 3)
      {
          usage();
          goto done;
      }

      sz = os_strlcpy(fs.fstype, argv[2], sizeof(fs.fstype));
      if (sz >= sizeof(fs.fstype))
      {
          fprintf(stderr, "Invalid fs type: '%s' (too long)\n", argv[2]);
          goto done;
      }

      err = fsd_unload(mh, &fs);
  }
  else if (strcmp(action, "create_local") == 0)
  {
      if (argc != 4)
      {
          usage();
          goto done;
      }

      err = fsd_fs_create_local(mh, argv[2], argv[3]);
  }
  else if (strcmp(action, "create_gfs") == 0)
  {
      fs_data_t fs;
      size_t sz;

      if (argc != 7)
      {
          usage();
          goto done;
      }

      COMPILE_TIME_ASSERT(sizeof("sfs") <= sizeof(fs.fstype));
      sz = os_strlcpy(fs.fstype, "sfs", sizeof(fs.fstype));

      sz = os_strlcpy(fs.devpath, argv[2], sizeof(fs.devpath));
      if (sz >= sizeof(fs.devpath))
      {
          fprintf(stderr, "Invalid dev path: '%s' (too long)\n", argv[2]);
          goto done;
      }

      sz = os_strlcpy(fs.clustered.gfs.lock_protocol, argv[3],
                      sizeof(fs.clustered.gfs.lock_protocol));
      if (sz >= sizeof(fs.clustered.gfs.lock_protocol))
      {
          fprintf(stderr, "Invalid lock protocol: '%s' (too long)\n", argv[3]);
          goto done;
      }

      if (to_uint64(argv[4], &fs.sizeKB) != EXA_SUCCESS)
      {
          fprintf(stderr, "Invalid size: '%s'\n", argv[4]);
          goto done;
      }

      sz = os_strlcpy(fs.clustered.gfs.uuid, argv[5], sizeof(fs.clustered.gfs.uuid));
      if (sz >= sizeof(fs.clustered.gfs.uuid))
      {
          fprintf(stderr, "Invalid GFS uuid: '%s' (too long)\n", argv[5]);
          goto done;
      }

      if (to_uint64(argv[6], &fs.clustered.gfs.nb_logs) != EXA_SUCCESS)
      {
          fprintf(stderr, "Invalid number of logs: '%s'\n", argv[6]);
          goto done;
      }

      err = fsd_fs_create_gfs(mh, &fs);
  }
  else if (strcmp(action, "dfinfo") == 0)
  {
      struct fsd_capa buf;

      if (argc != 3)
      {
          usage();
          goto done;
      }

      err = fsd_df(mh, argv[2], &buf);
      if (err == 0)
          printf("size=%"PRId64" bytes\n"
                 "used=%"PRId64" bytes\n"
                 "free=%"PRId64" bytes\n",
                 buf.size, buf.used, buf.free);
  }
  else if (strcmp(action, "resize") == 0)
  {
      uint64_t size_kb;

      if (argc != 6)
      {
          usage();
          goto done;
      }

      if (to_uint64(argv[5], &size_kb) != EXA_SUCCESS)
      {
          fprintf(stderr, "Invalid new size: '%s'\n", argv[5]);
          goto done;
      }

      err = fsd_resize(mh, argv[2], argv[3], argv[4], size_kb);
  }
  else if (strcmp(action, "prepare_resize") == 0)
  {
      if (argc != 4)
      {
          usage();
          goto done;
      }

      err = fsd_prepare_resize(mh, argv[2], argv[3]);
  }
  else if (strcmp(action, "read_shm") == 0)
  {
      read_shm();
  }
  else if (strcmp(action, "add_logs") == 0)
  {
      fs_data_t fs;
      int num_logs, actual_num_logs;
      size_t sz;

      if (argc != 4)
      {
          usage();
          goto done;
      }

      COMPILE_TIME_ASSERT(sizeof("sfs") <= sizeof(fs.fstype));
      os_strlcpy(fs.fstype, "sfs", sizeof(fs.fstype));

      sz = os_strlcpy(fs.devpath, argv[2], sizeof(fs.devpath));
      if (sz >= sizeof(fs.devpath))
      {
          fprintf(stderr, "Invalid dev path: '%s' (too long)\n", argv[2]);
          goto done;
      }

      if (to_int(argv[3], &num_logs) != EXA_SUCCESS)
      {
          fprintf(stderr, "Invalid number of logs: '%s'\n", argv[3]);
          goto done;
      }

      actual_num_logs = fsd_set_gfs_logs(mh, &fs, num_logs);
      if (actual_num_logs < 0)
          err = actual_num_logs;
      else
      {
	  examsgDelMbox(mh, EXAMSG_TEST_ID);
	  printf("Number of logs after the operation: %d\n", actual_num_logs);
      }
  }
  else
      usage();

  if (err != 0)
      fprintf(stderr, "Action finished with error %d: %s\n", err,
              exa_error_msg(-err));

done:

  examsgDelMbox(mh, EXAMSG_TEST_ID);

  if (mh != NULL)
      examsgExit(mh);

  if (static_init_ok)
      examsg_static_clean(EXAMSG_STATIC_RELEASE);

  return err == 0 ? 0 : 1;
}
