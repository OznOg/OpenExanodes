/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_UUID_H
#define EXA_UUID_H

#include "os/include/os_inttypes.h"

#define UUID_LEN 4
#define UUID_STR_LEN (8*4+3) /*4 elts of 8 bytes (32bits) + 3* ':'  */
typedef char exa_uuid_str_t[UUID_STR_LEN + 1];

/* Usage: printf("uuid:" UUID_FMT "\n", UUID_VAL(&uuid)) */
#define UUID_FMT "%08X:%08X:%08X:%08X"
#define UUID_VAL(uuid) (uuid)->id[0], (uuid)->id[1], (uuid)->id[2], (uuid)->id[3]

#ifdef __cplusplus
#include <iostream>

extern "C" {
#endif

/** Universal unique identifier (for devices, groups, ...) */
typedef struct exa_uuid {
  uint32_t	id[UUID_LEN];	/** id */
} exa_uuid_t;

extern const exa_uuid_t exa_uuid_zero;

bool uuid_is_equal (const exa_uuid_t *uuid1, const exa_uuid_t *uuid2);
int  uuid_compare (const exa_uuid_t *uuid1, const exa_uuid_t *uuid2);
bool uuid_is_zero (const exa_uuid_t *uuid);
void uuid_copy (exa_uuid_t *uuid_dest, const exa_uuid_t *uuid_src);
void uuid_zero (exa_uuid_t *uuid);
void uuid_generate (exa_uuid_t *uuid);

/**
 * Makes a string from an exa_uuid_t.
 *
 * Create a string from an UUID in order to be able to display it.
 * The function writes all char in the str_uuid string given in input,
 * and returns the pointer on the string.
 * The string format is of type :
 * 1A23CED1 <sep> 2BC123AE <sep> 1A23CED1<sep> 2BC123AE, ie in hexadecimal,
 * upper case letters separated by the char given as separator.
 *
 * @param[in]  uuid          The uuid we want to get the string version of.
 * @param[in]  sep           The separator to be used between uuid members.
 * @param[out] str_uuid      The destination string to be written
 *
 * @return     str_uuid or NULL if something went wrong
 */
char *uuid2str_with_sep(const exa_uuid_t *uuid, char sep, exa_uuid_str_t str_uuid);
#define uuid2str(uuid, str_uuid) uuid2str_with_sep((uuid), ':', (str_uuid))

/**
 * Extract a UUID from a string formated with uuid2str() style.
 * \sa uuid2str().
 *
 * @param[in]  str_uuid      The string to be scanned.
 * @param[in]  sep           The separator between uuid members.
 * @param[out] uuid	     The destination uuid to be written.
 *
 * @return     EXA_SUCCESS on success, -EXA_ERR_UUID.
 */
int uuid_scan_with_sep(const char *str_uuid, char sep, exa_uuid_t *uuid);
#define uuid_scan(str_uuid, uuid) uuid_scan_with_sep((str_uuid), ':', (uuid))

#ifdef __cplusplus
}
#endif

#endif /* EXA_UUID_H */
