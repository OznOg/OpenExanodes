/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "config/exa_version.h"
#include "common/include/exa_constants.h"
#include "git.h"

const char *get_version(void)
{
    return "Exanodes " EXA_EDITION_STR " " EXA_VERSION " for "
           EXA_PLATFORM_STR " (r"  GIT_REVISION  ")\n"
           EXA_COPYRIGHT "\n";
}


