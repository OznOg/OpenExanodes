/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADMIND__H
#define __ADMIND__H


/* FIXME: This looks very, very lonely. Maybe it could go
 * somewhere where it will have some friends to play with? */

typedef enum
{
    ADM_GOAL_PRESERVE       = 0,
    ADM_GOAL_CHANGE_CLUSTER = 1,
    ADM_GOAL_CHANGE_GROUP   = 2,
    ADM_GOAL_CHANGE_VOLUME  = 4 /* And FS too since the FS goal is the goal of the volume */
} adm_goal_change_t;

#endif
