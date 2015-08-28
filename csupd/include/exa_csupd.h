/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXA_CSUPD
#define H_EXA_CSUPD

/** \file exa_csupd.h
 * \brief Supervision daemon.
 *
 * Public header for communication with the supervision daemon
 */

#include "examsg/include/examsg.h"

/** Membership generation number */
typedef unsigned sup_gen_t;

/** Membership change event */
EXAMSG_DCLMSG(SupEventMshipChange, struct {
  sup_gen_t gen;        /**< Generation number */
  exa_nodeset_t mship;  /**< New membership */
});

#endif /* H_EXA_CSUPD */
