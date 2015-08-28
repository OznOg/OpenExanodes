/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include "os/include/os_random.h"

#include "common/include/uuid.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"

#include "os/include/os_stdio.h"



/* Uninitialized on purpose, so that it goes in the BSS. */
const exa_uuid_t exa_uuid_zero;

/**
 * Test whether two UUIDs are equal
 *
 * @param[in] uuid1 Pointer to first UUID
 * @param[in] uuid2 Pointer to second UUID
 *
 * @result true if equal, false otherwise
 */
bool uuid_is_equal (const exa_uuid_t *uuid1, const exa_uuid_t *uuid2)
{
  int i;

  for (i = 0 ; i < UUID_LEN ; i++)
    {
      if (uuid1->id[i] != uuid2->id[i])
	return false;
    }

  return true;
}

/**
 * Compare two uuids like strcmp does for strings
 *
 * @param[in] uuid1 Pointer to first UUID
 * @param[in] uuid2 Pointer to second UUID
 *
 * @result  1 if uuid1 > uuid2,
 *         -1 if uuid1 < uuid2 and
 *          0 if uuid1 = uuid2
 */
int uuid_compare (const exa_uuid_t *uuid1, const exa_uuid_t *uuid2)
{
  int i;

  for (i = 0 ; i < UUID_LEN ; i++)
    {
      if (uuid1->id[i] > uuid2->id[i])
	return 1;
      if (uuid1->id[i] < uuid2->id[i])
	return -1;
    }

  return 0;
}

/**
 * Test whether an UUIDs is null
 *
 * @param[in] uuid Pointer to the UUID
 *
 * @result true if zero, false otherwise
 */
bool uuid_is_zero (const exa_uuid_t *uuid)
{
  int i;

  for (i = 0 ; i < UUID_LEN ; i++)
    {
      if (uuid->id[i] != 0u)
	return false;
    }

  return true;
}

/**
 * Copy an UUID into another UUID
 *
 * @param[in] uuid_dest Pointer to destination UUID
 * @param[in] uuid_src  Pointer to source UUID
 *
 */
void uuid_copy (exa_uuid_t *uuid_dest, const exa_uuid_t *uuid_src)
{
  int i;

  for (i = 0 ; i < UUID_LEN ; i++)
    uuid_dest->id[i] = uuid_src->id[i];
}

/**
 * Zero an UUID
 *
 * @param[in] uuid UUID to zero
 */
void uuid_zero (exa_uuid_t *uuid)
{
  int i;

  for (i = 0 ; i < UUID_LEN ; i++)
    uuid->id[i] = 0;
}

char *uuid2str_with_sep(const exa_uuid_t *uuid, char sep, exa_uuid_str_t str_uuid)
{
    EXA_ASSERT(uuid);

    os_snprintf(str_uuid, sizeof(exa_uuid_str_t),
                "%08X%c%08X%c%08X%c%08X",
                uuid->id[0], sep, uuid->id[1], sep, uuid->id[2], sep, uuid->id[3]);
    return str_uuid;
}

int uuid_scan_with_sep(const char *str_uuid, char sep, exa_uuid_t *uuid)
{
  char sep1, sep2, sep3;
  char tmp[UUID_STR_LEN + 1];

  if (!str_uuid || !uuid)
    return -EXA_ERR_UUID;

  if (strnlen(str_uuid, UUID_STR_LEN + 1) != UUID_STR_LEN)
    return -EXA_ERR_UUID;

  strcpy(tmp, str_uuid);
  tmp[8]  = '\0';
  tmp[17] = '\0';
  tmp[26] = '\0';

  if (strspn(tmp + 0,  "0123456789ABCDEF") != 8 ||
      strspn(tmp + 9,  "0123456789ABCDEF") != 8 ||
      strspn(tmp + 18, "0123456789ABCDEF") != 8 ||
      strspn(tmp + 27, "0123456789ABCDEF") != 8)
    return  -EXA_ERR_UUID;

  if (sscanf(str_uuid, "%08X%c%08X%c%08X%c%08X", &uuid->id[0], &sep1,
             &uuid->id[1], &sep2, &uuid->id[2], &sep3, &uuid->id[3]) != 7)
      return -EXA_ERR_UUID;

  if (sep1 != sep || sep2 != sep || sep3 != sep)
    return -EXA_ERR_UUID;

  return EXA_SUCCESS;
}

/**
 * Generate a new UUID (randomly-generated).
 *
 * @param[out] uuid UUID to generate
 *
 * @note: os_random_init() must be called before the first call to uuid_generate().
 */
void
uuid_generate(exa_uuid_t *uuid)
{
    EXA_ASSERT_VERBOSE(os_random_is_initialized(), "os_random_init() must be "
                       "called before the first call to uuid_generate().");

    os_get_random_bytes(uuid, sizeof(exa_uuid_t));
}

