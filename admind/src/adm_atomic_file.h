/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_ATOMICFILE_H
#define __ADM_ATOMICFILE_H

#include <stdlib.h>

/**
 * Save a file atomically.
 *
 * @param[in] filename   Name of file to save/update
 * @param[in] data       New contents of file
 * @param[in] data_size  Size of contents
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int adm_atomic_file_save(const char *filename, const void *data,
                         size_t data_size);

#endif
