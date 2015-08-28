/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _NAMES_H
#define _NAMES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * General definition
 * ------------------
 */

#define DEV_ROOT_NAME			"exa"		// Defines /dev/exa/

/*
 * NBD module stuff
 */

#define NBD_MODULE_NAME                 "exa_bd"
#define EXA_RDEV_MODULE_NAME            "exa_rdev"



/******************************** should be clean bellow here *************************************************/
#ifndef KERNEL

/* TODO create a class for C++ use */
typedef enum exa_module_id {
#define EXA_MODULE__FIRST EXA_MODULE_RDEV   /* NOT_IN_PERL */
    EXA_MODULE_RDEV,
#ifndef WITH_BDEV
#define EXA_MODULE__LAST  EXA_MODULE_RDEV   /* NOT_IN_PERL */
#else
    EXA_MODULE_NBD,
#define EXA_MODULE__LAST  EXA_MODULE_NBD    /* NOT_IN_PERL */
#endif
} exa_module_id_t;


#define exa_module_check(id) ((id) >= EXA_MODULE__FIRST && (id) <= EXA_MODULE__LAST) /* NOT_IN_PERL */

const char *exa_module_name(exa_module_id_t id);

/* TODO create a class for C++ use */

typedef enum exa_daemon_id {
  EXA_DAEMON_ADMIND,
  EXA_DAEMON_FSD,
  EXA_DAEMON_SERVERD,
  EXA_DAEMON_CLIENTD,
  EXA_DAEMON_CSUPD,
  EXA_DAEMON_MSGD,
  EXA_DAEMON_LOCK_GULMD,
  EXA_DAEMON_MONITORD,
  EXA_DAEMON_AGENTX,
} exa_daemon_id_t;

#define EXA_DAEMON__FIRST EXA_DAEMON_ADMIND     /* NOT_IN_PERL */
#define EXA_DAEMON__LAST  EXA_DAEMON_AGENTX /* NOT_IN_PERL */

#define exa_daemon_check(id) ((id) >= EXA_DAEMON__FIRST && (id) <= EXA_DAEMON__LAST) /* NOT_IN_PERL */

const char *exa_daemon_name(exa_daemon_id_t id);
#endif

#ifdef __cplusplus
}
#endif

#endif
