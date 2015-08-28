/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef LUN_H
#define LUN_H

#include "os/include/os_inttypes.h"

/** SCSI Logical unit number */
typedef uint64_t lun_t;

/** Maximum number of LUNs */
#define MAX_LUNS  256

/** Symbolic value for 'no LUN' or 'invalid LUN' */
#define LUN_NONE  (MAX_LUNS + 5)

/** Check wether a LUN is valid */
#define LUN_IS_VALID(lun)  ((lun) < MAX_LUNS)

/** Minimum length of a string for it to be capable of holding any valid
   LUN */
#define LUN_STR_LEN  3

/** Format for printing a LUN */
#define PRIlun  PRIu64

/**
 * Convert a string to a LUN.
 *
 * @param[in] str  String to parse
 *
 * @return LUN if successful, LUN_NONE otherwise
 *         (use LUN_IS_VALID() to determine whether the conversion was
           successful)
 *
 * NOTE: This function is thread safe.
 */
lun_t lun_from_str(const char *str);

/**
 * Convert a LUN to a string.
 *
 * @param[in] lun  Lun to convert
 *
 * @return String if successful, NULL otherwise.
 *
 * NOTE: This function is thread safe.
 */
const char *lun_to_str(lun_t lun);


/**
 * Fill a buffer with bigendian version of LUN.
 *
 * @param[in]  lun     Lun to convert
 * @param[out] buffer  Buffer to write into
 */
void lun_set_bigendian(lun_t lun, unsigned char buffer[8]);

/**
 * Extract a LUN from a bigendian buffer.
 *
 * @param[in] buffer  Buffer to read from
 *
 * @return the LUN
 */
lun_t lun_get_bigendian(const unsigned char buffer[8]);

#endif /* LUN_H */
