/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_mem.h"

#include <stdlib.h>
#include <errno.h>
#include <malloc.h>

void *os_aligned_malloc(size_t size, size_t alignment, int *error_code)
{
    void *buffer = NULL;
    int ret;

    ret = posix_memalign(&buffer, alignment, size);
    if (ret != 0 && error_code)
        *error_code = ret;

    return buffer;
}

void __os_aligned_free(void *buffer)
{
  free(buffer);
}

