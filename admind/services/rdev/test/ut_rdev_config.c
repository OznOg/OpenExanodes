/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_random.h"

#include "admind/services/rdev/include/rdev_config.h"

#include "common/include/exa_constants.h"
#include "common/include/uuid.h"

#ifdef WIN32
#define putenv _putenv
#endif

ut_setup()
{
    os_random_init();
}

ut_cleanup()
{
    os_random_cleanup();
}

ut_test(no_config_dir_set)
{
    cl_error_desc_t err_desc;
    char *list;

    putenv("EXANODES_NODE_CONF_DIR");

    list = rdev_get_path_list(&err_desc);

    UT_ASSERT(list == NULL);
    UT_ASSERT_EQUAL(-EXA_ERR_BAD_INSTALL, err_desc.code);
}

static void set_config_path_for_test(const char *test)
{
   char *src_dir;

   /* Careful MUST be static to remain valid after function returns
    * (see man putenv) */
   static char path[sizeof("EXANODES_NODE_CONF_DIR") + 1 + OS_PATH_MAX];

   src_dir = getenv("srcdir");

   UT_ASSERT_VERBOSE(src_dir != NULL, "make sure env-key 'srcdir' is set and"
                     "points to this unit test's SOURCE directory !");

   os_snprintf(path, sizeof(path),
	       "EXANODES_NODE_CONF_DIR=%s" OS_FILE_SEP "rdev-data" OS_FILE_SEP "%s",
	       src_dir, test);
   putenv(path);
}

ut_test(no_disk_conf_file_returns_any)
{
   cl_error_desc_t err_desc;
   char *list;

   set_config_path_for_test("does_not_exist");

   list = rdev_get_path_list(&err_desc);

   UT_ASSERT_EQUAL_STR("any", list);

   os_free(list);
}

ut_test(empty_conf_file_returns_empty_string)
{
   cl_error_desc_t err_desc;
   char *list;

   set_config_path_for_test("test1");

   list = rdev_get_path_list(&err_desc);

   UT_ASSERT_EQUAL_STR("", list);

   os_free(list);
}

ut_test(disk_conf_file_correctly_read)
{
   cl_error_desc_t err_desc;
   char *list;

   set_config_path_for_test("test2");

   list = rdev_get_path_list(&err_desc);

   UT_ASSERT(list != NULL);

   UT_ASSERT_EQUAL_STR("/dev/sda /dev/sdb /dev/sdc", list);

   os_free(list);
}

ut_test(rdev_is_path_available)
{
   cl_error_desc_t err_desc;

   set_config_path_for_test("test2");

   UT_ASSERT(rdev_is_path_available("/dev/sda", &err_desc));
   UT_ASSERT(!rdev_is_path_available("/dev/pouet_pouet", &err_desc));
}
