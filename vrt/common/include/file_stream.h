/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#include "vrt/common/include/vrt_stream.h"

#include "os/include/os_stdio.h"

/**
 * Get a stream on an already opened file.
 *
 * WARNING - This function allocates memory.
 *
 * @param[out] stream   Stream created
 * @param      file     File on which to open a stream. The file is *not*
 *                      closed when the stream is closed.
 * @param[in]  access   Access mode (may restrict but not extend the file's
 *                      own access mode)
 *
 * @return 0 if successful, a negative error code otherwise
 */
int file_stream_on(stream_t **stream, FILE *file, stream_access_t access);

/**
 * Open a stream on a file.
 *
 * WARNING - This function allocates memory.
 *
 * NOTE: To open a block device with O_DIRECT access, first open the file
 * and set a properly aligned buffer, then use file_stream_on() on it.
 *
 * @param[out] stream   Stream created
 * @param[in] filename  Name of the file to open
 * @param[in] access    Access mode
 *
 * @return 0 if successful, a negative error code otherwise
 */
int file_stream_open(stream_t **stream, const char *filename, stream_access_t access);

#endif /* FILE_STREAM_H */
