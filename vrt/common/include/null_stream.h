/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef NULL_STREAM_H
#define NULL_STREAM_H

#include "vrt/common/include/vrt_stream.h"

/**
 * Open a seekable, write-only,  do-nothing stream (à la /dev/null).
 *
 * WARNING - This function allocates memory.
 *
 * @param[out] stream  Null stream created
 *
 * @return 0 if successful, a negative error code otherwise
 */
int null_stream_open(stream_t **stream);

#endif /* NULL_STREAM_H */
