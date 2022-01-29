/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_VERSION_H__
#define __EXA_VERSION_H__

#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"

#define EXA_VERSION_LEN  15

typedef struct { char v[EXA_VERSION_LEN + 1]; } exa_version_t;

/* FIXME TODO Add exa_version_valid() and exa_version_cmp() [to be used
              instead of os_strverscmp() wherever possible]. */

/**
 * Tell whether a version string is a major version.
 *
 * @param[in] str  Version string to check
 *
 * @return true if major, false otherwise
 */
bool exa_version_is_major(const exa_version_t *str);

/**
 * Extract the major version from a version string.
 *
 * @param[in]  version  Version to extract the major version from
 * @param[out] major    Extracted major version
 *
 * @return true if successful, false otherwise
 */
bool exa_version_get_major(const exa_version_t *version, exa_version_t *major);

/**
 * Copy an exa_version
 *
 * @param[in]   version_dest    Version to copy to
 * @param[in]   version_src     Version to copy
 *
 */
void exa_version_copy(exa_version_t *version_dest, const exa_version_t *version_src);

/**
 * Compare two versions
 *
 * @param[in]   vers1   First version
 * @param[in]   vers2   Second version
 *
 * @return true if both versions are equal, false otherwise
 */
bool exa_version_is_equal(const exa_version_t *vers1, const exa_version_t *vers2);

/**
 * Convert a strings to an exa version
 *
 * @param[in]   version Output of the conversion
 * @param[in]   src     Input string
 *
 * @return 0 if successful, negative error code otherwise
 */
int exa_version_from_str(exa_version_t *version, const char *src);

#endif /* __EXA_VERSION_H__ */
