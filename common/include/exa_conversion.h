/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CONVERSION_H__
#define __EXA_CONVERSION_H__

#ifdef __cplusplus
extern "C"
{
#endif


#include "os/include/os_inttypes.h"
#include "common/include/exa_error.h"

#ifdef WIN32
#define strtoll _strtoi64
#define strtoull _strtoui64
#endif

/**
   Convert a string to int64.

   @param str String to be converted.

   @param result The converted numerical value.
   @return 0 on successful conversion,
           -EINVAL if an invalid content is found in string
                     (strings starting with space or base prefix are valid),
	   -ERANGE if converted value is out of type range.
*/
int to_int64(const char *str, int64_t *result);

/**
   Convert a string to uint64.

   @param str String to be converted.

   @param result The converted numerical value.
   @return 0 on successful conversion,
           -EINVAL if an invalid content is found in string
                     (strings starting with space or base prefix are valid),
	   -ERANGE if converted value is out of type range.
*/
int to_uint64(const char *str, uint64_t *result);

/**
   Convert a string to int32.

   @param str String to be converted.

   @param result The converted numerical value.
   @return 0 on successful conversion,
           -EINVAL if an invalid content is found in string
                     (strings starting with space or base prefix are valid),
	   -ERANGE if converted value is out of type range.
*/
int to_int32(const char *str, int32_t *result);

/**
   Convert a string to uint32.

   @param str String to be converted.

   @param result The converted numerical value.
   @return 0 on successful conversion,
           -EINVAL if an invalid content is found in string
                     (strings starting with space or base prefix are valid),
	   -ERANGE if converted value is out of type range.
*/
int to_uint32(const char *str, uint32_t *result);

/**
 * Convert a string to uint16.
 *
 * @param[in]  str     String to be converted
 * @param[out] result  Resulting value
 *
 * @return 0 on successful conversion,
 *         -EINVAL if an invalid content is found in string
 *                 (strings starting with space or base prefix are valid),
 *         -ERANGE if the resulting value is out of the type range
 */
int to_uint16(const char *str, uint16_t *result);

/**
 * Convert a string to uint8.
 *
 * @param[in]  str     String to be converted
 * @param[out] result  Resulting value
 *
 * @return 0 on successful conversion,
 *         -EINVAL if an invalid content is found in string
 *                 (strings starting with space or base prefix are valid),
 *         -ERANGE if the resulting value is out of the type range
 */
int to_uint8(const char *str, uint8_t *result);

/**
 * Convert a string to int.
 *
 * @param[in]  str     String to be converted
 * @param[out] result  Resulting int
 *
 * @return 0 on successful conversion,
 *         -EINVAL if an invalid content is found in string
 *                   (strings starting with space or base prefix are valid),
 *         -ERANGE if converted value is out of type range.
 */
int to_int(const char *str, int *result);

/**
 * Convert a string to a unsigned int.
 *
 * @param[in]  str     String to be converted
 * @param[out] result  Resulting unsigned int
 *
 * @return 0 on successful conversion,
 *         -EINVAL if an invalid content is found in string
 *                   (strings starting with space or base prefix are valid),
 *         -ERANGE if converted value is out of type range.
 */
int to_uint(const char *str, unsigned int *result);

/** \brief Given a string number (size) with a byte factor suffix, return
 * an integer size in kiloBytes.
 *
 * Suffix unit are not case sensitive. Suffix unit is mandatory.
 * Supported suffixes unit are K, M, G, T, P, E (From Kilo to Exa)
 * Suffixes unit can be terminated by the letter B or b
 * The decimal point is accepted like in 1.2TB.
 *
 * For example:
 *  '1234MB' '1234 MB' '1234M' '1234m'  => 1234
 *
 * @param orig_size_string      A size string with a suffix.
 * @param result The converted numerical value.
 *
 * @return 0 on sucessful conversion, -EINVAL or -ERANGE.
 */
int exa_get_size_kb(const char *size_string, uint64_t *result);

char *exa_get_human_size(char *size_str, size_t n, uint64_t size_kb);

/**
 * Converts a number of kilobytes to a number of sectors.
 *
 * @param[in] kb        number of kilobytes in uint32_t
 * @param[out] sector   number of sectors in uint32_t
 *
 * @return 0 upon success.
 *         -ERANGE if number of sectors exceeds the range of uint32_t
 */
int kb_to_sector_uint32(const uint32_t kb, uint32_t *sector);

/**
 * Converts a number of kilobytes to a number of sectors.
 *
 * @param[in] kb        number of kilobytes in uint64_t
 * @param[out] sector   number of sectors in uint64_t
 *
 * @return 0 upon success.
 *         -ERANGE if number of sectors exceeds the range of uint64_t
 */
int kb_to_sector_uint64(const uint64_t kb, uint64_t *sector);

#ifdef __cplusplus
}
#endif

#endif /* __EXA_CONVERSION_H__ */
