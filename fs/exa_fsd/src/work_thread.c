/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "os/include/strlcpy.h"
#include "common/include/threadonize.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"

#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "common/include/daemon_request_queue.h"
#include "os/include/os_stdio.h"

#include "fs/include/exa_fsd.h"
#include "fs/include/exactrl-fs.h"
#include "fs/exa_fsd/src/exa_fsd.h"
#include "fs/exa_fsd/src/fscommands.h"

#define FSD_THREAD_STACK_SIZE 300000

static os_thread_t fs_thread[EXA_FS_REQUEST_LAST];

/* --- fsd_handle_fs_request -------------------------------------- */
/* Handle a request to the exa_fsd daemon by calling the correct
 * funtion.
 *
 * \param[in] queue     Request queue handler.
 * \param[in] req	the request.
 * \param[in] from	Sender of the request
 * \return 0 on success or an error code.
 */
static void
fsd_handle_fs_request(struct daemon_request_queue *queue,
	              const FSRequest *req, ExamsgID from)
{
  int ret = 0;
  FSAnswer answer;

  EXA_ASSERT(req);
  EXA_ASSERT(queue);

  /* initialize answer */
  memset (&answer, 0, sizeof(answer));

  switch (req->requesttype)
    {
      case FSREQUEST_FSINFO:
        exalog_debug("fsinfo handler");
        ret = fsd_do_fsinfo(req->argument.fsinfo.devpath);
        break;
      case FSREQUEST_MOUNTPOINTINFO:
        exalog_debug("mountpointinfo handler");
        ret = fsd_do_mountpointinfo(req->argument.mountpointinfo.mountpoint);
        break;
      case FSREQUEST_DF_INFO:
        {
          exalog_debug("dfinfo handler");
          ret = fsd_do_dfinfo(req->argument.dfinfo.mountpoint,
			      &answer.capa);
        }
        break;
      case FSREQUEST_PREPARE:
        exalog_debug("prepare handler");
        ret = fsd_do_prepare(&req->argument.startstop.fs);
        break;
      case FSREQUEST_MOUNT:
        exalog_debug("mount handler");
        ret = fsd_do_mount(&req->argument.startstop.fs,
			   req->argument.startstop.read_only,
			   req->argument.startstop.mount_remount);
	break;
      case FSREQUEST_POSTMOUNT:
        exalog_debug("postmount handler");
	fsd_do_post_mount(&req->argument.startstop.fs,
			  req->argument.startstop.group_name,
			  req->argument.startstop.fs_name);
	ret = EXA_SUCCESS;
        break;
      case FSREQUEST_PREUMOUNT:
        exalog_debug("preumount handler");
	fsd_do_pre_umount(&req->argument.startstop.fs,
			  req->argument.startstop.group_name,
			  req->argument.startstop.fs_name);
	ret = EXA_SUCCESS;
        break;
      case FSREQUEST_UMOUNT:
        exalog_debug("umount handler");
        ret = fsd_do_umount(&req->argument.startstop.fs);
        break;
      case FSREQUEST_UNLOAD:
        exalog_debug("unload handler");
        ret = fsd_do_unload(&req->argument.startstop.fs);
        break;
      case FSREQUEST_CREATEGFS:
        exalog_debug("creategfs handler");
        ret = fsd_do_creategfs(&req->argument.creategfs.fs);
        break;
      case FSREQUEST_CREATELOCAL:
        exalog_debug("createlocal handler");
        ret = fsd_do_createlocal(req->argument.createlocal.fstype,
				 req->argument.createlocal.devpath);
        break;
      case FSREQUEST_RESIZE:
        exalog_debug("resize handler");
        ret = fsd_do_resize(req->argument.resize.fstype,
			    req->argument.resize.devpath,
			    req->argument.resize.mountpoint,
			    req->argument.resize.action,
			    req->argument.resize.sizeKB);
        break;
      case FSREQUEST_CHECK:
        exalog_debug("check handler");
        ret = fsd_do_check(&req->argument.check.fs,
			   req->argument.check.optional_parameters,
			   req->argument.check.repair,
			   req->argument.check.output_file);
        break;
      case FSREQUEST_UPDATE_GFS_CONFIG:
        exalog_debug("update gfs config handler");
        ret = fsd_do_update_gfs_config();
        break;
      case FSREQUEST_UPDATE_CMAN:
        exalog_debug("update cman handler");
        ret = fsd_do_update_cman(req->argument.updatecman.nodename);
        break;
      case FSREQUEST_SET_LOGS:
        exalog_debug("set the number of logs in gfs");
        ret = fsd_do_set_gfs_logs(&req->argument.setlogs.fs,
				  req->argument.setlogs.number_of_logs,
				  &answer.number_of_logs);
        break;
      case FSREQUEST_UPDATE_GFS_TUNING:
        exalog_debug("set the tuning in gfs");
        ret = fsd_do_update_gfs_tuning(&req->argument.updategfstuning.fs);
        break;
      default:
        EXA_ASSERT(false);
    }

  answer.request = req->requesttype;
  answer.ack = ret;
  EXA_ASSERT(daemon_request_queue_reply(mh, from, queue, &answer,
					sizeof(answer)) == 0);
}

/* --- fsd_workthread_run ----------------------------------------- */
/**
 * Start the thread
 *
 * \return void
 */
static void
fsd_workthread_run(void * arg)
{
  struct daemon_request_queue *queue = arg;

  exalog_as(EXAMSG_FSD_ID);

  while (1)
    {
      FSRequest req;
      ExamsgID from;

      /* fsd_queue_get_request() blocks until a request income */
      daemon_request_queue_get_request(queue, &req, sizeof(req), &from);

      /* handle the request and send the reply */
      fsd_handle_fs_request(queue, &req, from);
    }
}

/* --- fsd_workthread_init ---------------------------------------- */
/**
 * Initialize threads
 *
 * \return void
 */
void
fsd_workthread_init(void)
{
  int i;
  for (i = 0; i < EXA_FS_REQUEST_LAST; ++i)
    {
      char thread_name[32];
      char queue_name[EXA_MAXSIZE_REQUEST_QUEUE_NAME+1];

      os_snprintf(thread_name, sizeof(thread_name), "exa_fsd_%d", i);
      os_snprintf(queue_name, sizeof(queue_name), "queue_%d", i);

      /* Initialize queue */
      requests[i] = daemon_request_queue_new(queue_name);
      EXA_ASSERT(requests[i]);

      exathread_create_named (&fs_thread[i],
          FSD_THREAD_STACK_SIZE+MIN_THREAD_STACK_SIZE,
          & fsd_workthread_run, (void *)(requests[i]),
          thread_name);
    }
}

