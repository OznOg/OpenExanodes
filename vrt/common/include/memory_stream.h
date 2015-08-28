/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef MEMORY_STREAM_H
#define MEMORY_STREAM_H

#include "vrt/common/include/vrt_stream.h"

/**
 * Open a stream on a buffer.
 *
 * WARNING - This function allocates memory.
 *
 * @param[out] stream  Stream created
 * @param      bytes   Buffer to open a stream on
 * @param[in]  size    Buffer size, in bytes
 * @param[in]  access  Access mode
 *
 * @return 0 if successful, a negative error code otherwise
 */
int memory_stream_open(stream_t **stream, char *bytes, size_t size,
                       stream_access_t access);

#endif /* MEMORY_STREAM_H */
