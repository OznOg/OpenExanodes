/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXA_FSD
#define H_EXA_FSD

#include "examsg/include/examsg.h"
#include "common/include/daemon_request_queue.h"

/* === Internal API ================================================== */

extern ExamsgHandle mh;

/* there is 3 request types:
 * - requests for clinfo (0)
 * - requests for the CLI (1)
 * - requests for the recovery (2)
 * - requests from the test program (NUM_REQUEST_TEST)
 */
enum exa_fs_request_type_t {
  EXA_FS_REQUEST_INFO,
  EXA_FS_REQUEST_CMD,
  EXA_FS_REQUEST_RECOVERY,
  EXA_FS_REQUEST_TEST,
  EXA_FS_REQUEST_LAST,
};

extern char hostname[];

extern struct daemon_request_queue *requests[EXA_FS_REQUEST_LAST];

extern void gfs_set_gulm_running(bool running);

#endif /* H_EXA_FSD */
