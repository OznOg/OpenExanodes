/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SUP_DEBUG_H__
#define __SUP_DEBUG_H__

#ifdef WITH_SUPSIM

#include <stdlib.h>

#define init_log()

int __exalog_text(int unused_error_level, const char *unused_file,
		  const char *unused_fun, unsigned unused_line,
		  const char *fmt, ...);

#define __debug(...)  __exalog_text(0, NULL, NULL, 0, __VA_ARGS__)
#define __trace(...)  __exalog_text(0, NULL, NULL, 0, __VA_ARGS__)
#define __error(...)  __exalog_text(0, NULL, NULL, 0, __VA_ARGS__)

#else /* !WITH_SUPSIM */

#include "log/include/log.h"

#define init_log() do { examsg_static_init(EXAMSG_STATIC_GET); \
                        exalog_static_init(); \
                        exalog_as(EXAMSG_CSUPD_ID); } while (0)

#define __debug(...)  exalog_debug(__VA_ARGS__)
#define __trace(...)  exalog_trace(__VA_ARGS__)
#define __error(...)  exalog_error(__VA_ARGS__)

#endif /* WITH_SUPSIM */

#endif /* __SUP_DEBUG_H__ */
