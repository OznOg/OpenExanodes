/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/* Private header shared between the parent side and the child side of the
 * daemon library. */

#ifndef OS_DAEMON_COMMON_H
#define OS_DAEMON_COMMON_H

/** Fd used by the child (daemon) to send its post-initialization
    status to its parent. */
#define DAEMON_CHILD_STATUS_FD  5

#endif /* OS_DAEMON_COMMON_H */
