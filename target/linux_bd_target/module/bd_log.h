/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef BD_LOG_H
#define BD_LOG_H

#define bd_log_debug(fmt, ...)  do {} while (0)
#define bd_log_error printk
#define bd_log_warning printk
#define bd_log_info(fmt, ...)  do {} while (0)

#endif
