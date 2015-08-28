/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "common/include/exa_conversion.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h> /* strtoll */
#include <limits.h>

#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"

static const uint64_t I_KILO = 0x400ULL;
static const uint64_t I_MEGA = 0x100000ULL;
static const uint64_t I_GIGA = 0x40000000ULL;
static const uint64_t I_TERA = 0x10000000000ULL;
static const uint64_t I_PETA = 0x4000000000000ULL;

int to_int64(const char *str, int64_t *result)
{
    int64_t tmp;
    char *endptr;

    if (str == NULL || result == NULL)
        return -EINVAL;

    errno = 0;
    /* Check that long long fits in int64 */
    assert(sizeof(long long) <= 8);  /* 8 * 8 = 64 */

    /* strtoll() doesn't mind if there are leading spaces, tabs, newlines,
       but we do */
    if (isspace(str[0]))
        return -EINVAL;

    tmp = strtoll(str, &endptr, 0);

    if (*str == '\0')
	return -EINVAL;
    if (*str == *endptr)
	return -EINVAL;
    if (*endptr != '\0')
	return -EINVAL;
    if (errno == ERANGE)
	return -errno;

    *result = tmp;
    return EXA_SUCCESS;
}

int to_uint64(const char *str, uint64_t *result)
{
    uint64_t tmp;
    const char *startptr = str;
    char *endptr;

    if (str == NULL || result == NULL)
        return -EINVAL;

    errno = 0;
    /* Check that unsigned long long fits in uint64 */
    assert(sizeof(unsigned long long) <= 8); /* 8 * 8 = 64 */

    /* strtoull() doesn't mind if there are leading spaces, tabs, newlines,
       but we do */
    if (isspace(str[0]))
        return -EINVAL;

    tmp = strtoull(str, &endptr, 0);

    if (*str == '\0')
	return -EINVAL;
    if (*str == *endptr)
	return -EINVAL;
    if (*endptr != '\0')
	return -EINVAL;
    if (errno == ERANGE)
	return -errno;

    /* strtoul does not return an ERANGE if string contains a
     * negative number. must be done manually... */
    while (isspace(*startptr))
	++startptr;
    if (*startptr == '-')
	return -ERANGE;

    *result = tmp;
    return EXA_SUCCESS;
}

int to_int32(const char *str, int32_t *result)
{
    int64_t tmp;
    int err = to_int64(str, &tmp);

    if (err != EXA_SUCCESS)
	return err;
    if ((tmp < INT32_MIN) || (tmp > INT32_MAX))
	return -ERANGE;

    *result = (int32_t) tmp;
    return EXA_SUCCESS;
}

int to_uint32(const char *str, uint32_t *result)
{
    int64_t tmp;
    int err = to_int64(str, &tmp);

    if (err != EXA_SUCCESS)
	return err;

    if ((tmp < 0) || (tmp > UINT32_MAX))
	return -ERANGE;

    *result = (uint32_t) tmp;
    return EXA_SUCCESS;
}

int to_uint16(const char *str, uint16_t *result)
{
    int64_t tmp;
    int err = to_int64(str, &tmp);

    if (err != EXA_SUCCESS)
        return err;

    if (tmp < 0 || tmp > UINT16_MAX)
        return -ERANGE;

    *result = (uint16_t)tmp;
    return EXA_SUCCESS;
}

int to_uint8(const char *str, uint8_t *result)
{
    int64_t tmp;
    int err = to_int64(str, &tmp);

    if (err != EXA_SUCCESS)
        return err;

    if (tmp < 0 || tmp > UINT8_MAX)
        return -ERANGE;

    *result = (uint8_t)tmp;
    return EXA_SUCCESS;
}

int to_int(const char *str, int *result)
{
    int64_t tmp;
    int err;

    err = to_int64(str, &tmp);
    if (err != EXA_SUCCESS)
        return err;

    if (tmp < INT_MIN || tmp > INT_MAX)
        return -ERANGE;

    *result = (int)tmp;

    return EXA_SUCCESS;
}

int to_uint(const char *str, unsigned int *result)
{
    int64_t tmp;
    int err;

    err = to_int64(str, &tmp);
    if (err != EXA_SUCCESS)
	return err;

    if (tmp < 0 || tmp > UINT_MAX)
	return -ERANGE;

    *result = (unsigned int)tmp;

    return EXA_SUCCESS;
}

int exa_get_size_kb(const char *orig_size_string, uint64_t *result)
{
    char     *size_string;
    double size_request;
    uint64_t sizeKB = 0;
    char suffix;
    char     *endptr;
    int ret = EXA_SUCCESS;

    /* Really expect a string */
    if (orig_size_string == NULL)
        return -EINVAL;

    size_string = os_strdup(orig_size_string);

    /* Split the number and the suffix */
    suffix = size_string[strlen(size_string) - 1];

    size_string[strlen(size_string) - 1] = '\0';

    errno = 0;
    size_request = strtod(size_string, &endptr);

    /* If there are remaining chars to parse, it's not a valid number */
    if (size_string == endptr || *endptr != '\0')
        ret = -EINVAL;

    os_free(size_string);

    if (ret != EXA_SUCCESS)
        return ret;
    if (size_request < 0)
        return -ERANGE;
    suffix = tolower(suffix);

    /* Transform size in KILOBYTE */
    if (suffix == 'e')
    {
        size_request = size_request * I_PETA;
    }
    else if (suffix == 'p')
    {
        size_request = size_request * I_TERA;
    }
    else if (suffix == 't')
    {
        size_request = size_request * I_GIGA;
    }
    else if (suffix == 'g')
    {
        size_request = size_request * I_MEGA;
    }
    else if (suffix == 'm')
    {
        size_request = size_request * I_KILO;
    }
    else if (suffix == 'k')
    {
        sizeKB = (uint64_t)size_request;
        /* Final round to have 1.6K => 2K */
        if (size_request - sizeKB > 0.5)
            sizeKB++;

        *result = sizeKB;
        return EXA_SUCCESS;
    }
    else
    {
        /* Unknown suffix */
        return -EINVAL;
    }

    /* Check there will be no overflow */
    if (size_request <= UINT64_MAX)
    {
        sizeKB = (uint64_t)size_request;
    }
    else
    {
        return -ERANGE;
    }

    *result = sizeKB;
    return EXA_SUCCESS;
}


/**
 * \brief Given a kilobyte number return an equivalent string in a
 *        human readable form.
 * \param[out] size_str String to fill with the human readable size
 * \param[in]  n        Size of the size_str string
 * \param[in]  size_kb  Size to convert (in KB)
 * \return Size in human readable format
 */
char *exa_get_human_size(char *size_str, size_t n, uint64_t size_kb)
{
    uint64_t size_request = size_kb;

    if (size_kb == 0)
    {
        os_snprintf(size_str, n, "%s", "0 K");
        return size_str;
    }

    if (size_request >= I_PETA)
    {
        os_snprintf(size_str, n, "%.2f E", (double) size_request / I_PETA);
    }
    else if (size_request >= I_TERA)
    {
        os_snprintf(size_str, n, "%.2f P", (double) size_request / I_TERA);
    }
    else if (size_request >= I_GIGA)
    {
        os_snprintf(size_str, n, "%.2f T", (double) size_request / I_GIGA);
    }
    else if (size_request >= I_MEGA)
    {
        os_snprintf(size_str, n, "%.2f G", (double) size_request / I_MEGA);
    }
    else if (size_request >= I_KILO)
    {
        os_snprintf(size_str, n, "%.2f M", (double) size_request / I_KILO);
    }
    else
    {
        os_snprintf(size_str, n, "%.2f K", (double) size_request);
    }

    return size_str;
}


int kb_to_sector_uint32(const uint32_t kb, uint32_t *sector)
{
    if (kb & (1UL << 31))
        return -ERANGE;
    *sector = kb << 1;
    return EXA_SUCCESS;
}


int kb_to_sector_uint64(const uint64_t kb, uint64_t *sector)
{
    if (kb & (1ULL << 63))
        return -ERANGE;
    *sector = kb << 1;
    return EXA_SUCCESS;
}

