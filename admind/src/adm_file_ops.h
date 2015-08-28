/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_FILE_OPS_H
#define __ADM_FILE_OPS_H

#include "common/include/exa_error.h"
#include "admind/src/adm_error.h"

/**
 * Return the raw content of a text file and returns its contents in a
 * NULL-terminated string.
 * The returned buffer MUST be freed by the caller by calling os_free().
 *
 * @param[in]  path        the file path to load
 * @param[out] error_desc  Error descriptor
 *
 * @return '\0' terminated buffer of the raw file contents
 */
char *adm_file_read_to_str(const char *path, cl_error_desc_t *err_desc);

/**
 * Writes a given NULL-terminated string to a file.
 *
 * @param[in]  path        the file path to load
 * @param[in]  contents    the contents to write
 * @param[out] error_desc  Error descriptor
 *
 */
void adm_file_write_from_str(const char *path, const char *contents, 
                             cl_error_desc_t *err_desc);
#endif /* __ADM_FILE_OPS_H */
