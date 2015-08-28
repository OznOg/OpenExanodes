/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libxml/parser.h>

#include "os/include/os_file.h"
#include "os/include/os_string.h"

#include "admind/src/adm_deserialize.h"
#include "admind/src/adm_serialize.h"
#include "admind/src/adm_cluster.h"
#include "common/include/exa_error.h"
#include <unit_testing.h>

/*
 * TODO Must add unit tests with invalid parameters.
 */

/* FIXME this is really a hugly hack not to link with services lib
 * It relies on the fact that struct fs_definition begins with the name
 * array and that the adm_deserialize uses this array only in structure
 * It would be nice for the deserialize file not to include service fs
 * files...*/
char **fs_get_definition(void)
{
  static char * names[] = { "ext3", "sfs", NULL };

  return names;
}

static char error_msg[EXA_MAXSIZE_LINE + 1];
static char buffer[16];

/* Helper */
static bool __deserialize_from_file(const char *filename)
{
    int ret;

    /* Reset cluster is mandatory because adm_xxx() has side-effects, working
     * on a global, hidden cluster variable */
    adm_cluster_cleanup();

    ret = adm_deserialize_from_file(filename, error_msg, false /* create */);
    if (ret != EXA_SUCCESS)
    {
        ut_printf("adm_deserialize_from_file() failed: %s\n", error_msg);
        return false;
    }

    return true;
}

UT_SECTION(load_to_memory)

ut_setup()
{
  xmlInitParser();
  adm_cluster_init();
}

ut_cleanup()
{
  adm_cluster_cleanup();
  xmlCleanupParser();
}

ut_test(deserialize_from_file)
{
    UT_ASSERT(__deserialize_from_file("test.conf.in"));
}

ut_test(serialize_to_null)
{
  int ret;

  UT_ASSERT(__deserialize_from_file("test.conf.in"));

  ret = adm_serialize_to_null(false /* create */);
  if (ret < EXA_SUCCESS)
  {
    ut_printf("adm_serialize_to_null() failed: %s\n", exa_error_msg(ret));
    UT_FAIL();
  }
}

ut_test(serialize_to_memory)
{
  int ret;

  UT_ASSERT(__deserialize_from_file("test.conf.in"));

  ret = adm_serialize_to_memory(buffer, sizeof(buffer), false /* create */);
  if (ret != -ENOSPC)
  {
    ut_printf("adm_serialize_to_memory() returned %d\n", ret);
    UT_FAIL();
  }
}
