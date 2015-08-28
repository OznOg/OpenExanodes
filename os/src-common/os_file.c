/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include "os/include/os_file.h"

char *os_dirname(char *path)
{
    /* This is reimplemented because including <libgen.h>
     * changes the basename() implementation, and we rely
     * on the "normal" one.
     */

    if (path == NULL)
        return ".";

    if (!strchr(path, *OS_FILE_SEP))
            return ".";

    *strrchr(path, *OS_FILE_SEP) = '\0';

    return path;
}
