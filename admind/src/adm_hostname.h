/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_HOSTNAME_H__
#define __ADM_HOSTNAME_H__

const char *adm_hostname(void);

void adm_hostname_override(const char *hostname);
void adm_hostname_reset(void);

int adm_hostname_load(char *hostname);
int adm_hostname_save(const char *hostname);
int adm_hostname_delete_file(void);

#endif
