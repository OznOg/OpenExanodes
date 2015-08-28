/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_CACHE_H
#define __ADM_CACHE_H

/**
 * This function tries to remove every file and subdirectory in Exanodes'
 * cache directory and the top-level cache directory itself.
 *
 * NOTE: Returns nothing, because its work is best-effort only.
 */
void adm_cache_cleanup(void);

/**
 * This function creates the top-level cache directory of Exanodes.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int adm_cache_create(void);

#endif
