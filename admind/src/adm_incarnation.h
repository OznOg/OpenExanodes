/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_INCARNATION_H__
#define __ADM_INCARNATION_H__

extern unsigned short incarnation;

int adm_set_incarnation(void);
int adm_delete_incarnation(void);

#endif
