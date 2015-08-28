/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include <string.h>

#include "os/include/strlcpy.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_tools.h"

/**************************************************************/
UT_SECTION(exaperf_config_remove_white_spaces)
char str[128];

ut_test(no_line_feed)
{
    strncpy(str, "hello", sizeof(str));
    remove_whitespace(str);
    UT_ASSERT_EQUAL_STR("hello", str);
}

ut_test(one_line_feed)
{
    strncpy(str, "hello\n", sizeof(str));
    remove_whitespace(str);
    UT_ASSERT_EQUAL_STR("hello", str);
}

ut_test(two_line_feeds)
{
    strncpy(str, "hello\nhello\n", sizeof(str));
    remove_whitespace(str);
    UT_ASSERT_EQUAL_STR("hello hello", str);
}

ut_test(badly_formatted_string)
{
    strncpy(str, "  coucou  = toto   titi   ", sizeof(str));
    remove_whitespace(str);
    UT_ASSERT_EQUAL_STR("coucou = toto titi", str);
}

ut_test(well_formatted_string)
{
    strncpy(str, "coucou = toto titi", sizeof(str));
    remove_whitespace(str);
    UT_ASSERT_EQUAL_STR("coucou = toto titi", str);
}

/*************************************************************/
UT_SECTION(exaperf_config_last_character)
char str[128];
const char *last_char;

ut_test(normal)
{
    strncpy(str, "abcdef", sizeof(str));
    last_char = last_character(str);
    UT_ASSERT_EQUAL('f', *last_char);
}
