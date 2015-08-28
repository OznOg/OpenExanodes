/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_CHECK_H__
#define __RAIN1_CHECK_H__

/**
 * Fully reset the devices used in the given group.
 *
 * @param[in] private_data      The layout's private data
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
int rain1_group_reset(void *private_data);

/**
 * Check the devices of the given group.
 * The devices must have been resetted in the past.
 *
 * @param[in] private_data      The layout's private data
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
int rain1_group_check(void *private_data);

#endif
