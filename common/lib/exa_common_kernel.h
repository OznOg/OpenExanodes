/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef _EXA_COMMON_KERNEL_H
#define _EXA_COMMON_KERNEL_H

/*
 * Common module stuff
 */
#define EXACOMMON_MODULE_NAME "exa_common"
#define EXACOMMON_MODULE_PATH "/dev/" EXACOMMON_MODULE_NAME
#define EXA_SET_NAME   0x61
#define EXA_NOIO       0x62
#define EXA_SELECT_IN  0x63
#define EXA_SELECT_OUT 0x64
#define EXA_SELECT_MAL 0x65
#define EXA_EMERGENCY_SIZE 0x66

#endif /* _EXA_COMMON_KERNEL_H */
