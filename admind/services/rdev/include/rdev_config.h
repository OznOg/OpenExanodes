/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_error.h"

/**
 * Returns a new allocated string that contains the list of paths
 * allowed to be used on the node. If no such list was filled by
 * the administrator, the string "any" is returned.
 * The output format is "path1 path2 path3".
 *
 * WARNING this function allocates memory, and the returned buffer
 *         MUST be freed by the caller with os_free.
 *
 * @param[in,out] err_desc  An error descriptor for output.
 *
 * @return disk path list or "any", NULL on error
 */
char *rdev_get_path_list(cl_error_desc_t *err_desc);


/**
 * Tells if a disk can be used by Exanodes.
 *
 * @param[in]     path       path to be tested.
 * @param[in,out] err_desc   an error descriptor for output.
 *
 * @return true if the path can be used by Exanodes.
 */
bool rdev_is_path_available(const char *path, cl_error_desc_t *err_desc);
