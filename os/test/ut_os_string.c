/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "os/include/os_string.h"
#include "os/include/strlcpy.h"

#include <ctype.h>

UT_SECTION(test_strtok)

ut_test(null_delimiters)
{
    char str[32] = "dummy string";
    char *ptr;
    UT_ASSERT(os_strtok(str, NULL, &ptr) == NULL);
}

ut_test(null_save_ptr)
{
    char str[32] = "bonjour chez vous";
    UT_ASSERT(os_strtok(str, " ", NULL) == NULL);
}

ut_test(empty_save_ptr)
{
    char *ptr = "";
    UT_ASSERT(os_strtok(NULL, "f", &ptr) == NULL);
}

ut_test(parsing)
{
    char string[] = "I'm not there";
    char *ptr = string;
    char *tok;

    tok = os_strtok(string, " ", &ptr);
    UT_ASSERT(tok != NULL && strcmp(tok, "I'm") == 0);

    tok = os_strtok(NULL, " ", &ptr);
    UT_ASSERT(tok != NULL && strcmp(tok, "not") == 0);

    tok = os_strtok(NULL, " ", &ptr);
    UT_ASSERT(tok != NULL && strcmp(tok, "there") == 0);

    tok = os_strtok(NULL, " ", &ptr);
    UT_ASSERT(tok == NULL);
}

UT_SECTION(os_strcasecmp)

ut_test(empty_string_equals_itself)
{
    UT_ASSERT(os_strcasecmp("", "") == 0);
}

ut_test(non_empty_string_equals_itself)
{
    const char *str = "hello, world";
    UT_ASSERT(os_strcasecmp(str, str) == 0);
}

ut_test(empty_string_before_non_empty_string)
{
    UT_ASSERT(os_strcasecmp("", " ") < 0);
    UT_ASSERT(os_strcasecmp("", "0") < 0);
    UT_ASSERT(os_strcasecmp("", "a") < 0);
    UT_ASSERT(os_strcasecmp("", "A") < 0);
}

ut_test(different_strings_are_not_equal)
{
    const char *str1 = "hello!";
    const char *str2 = "hello?";

    UT_ASSERT(os_strcasecmp(str1, str2) != 0);
}

ut_test(lowercase_equals_uppercase)
{
#define LOREM_IPSUM \
        "Lorem ipsum dolor sit amet, consectetur"                       \
        "adipiscing elit. Mauris consectetur tortor"                    \
        "ac leo pretium non facilisis ligula convallis."
    const char *str = LOREM_IPSUM;
    char u_str[sizeof(LOREM_IPSUM)];
    char l_str[sizeof(LOREM_IPSUM)];
    int i;

    for (i = 0; str[i] != '\0'; i++)
    {
        u_str[i] = toupper(str[i]);
        l_str[i] = tolower(str[i]);
    }
    u_str[i] = '\0';
    l_str[i] = '\0';

    UT_ASSERT(os_strcasecmp(str, u_str) == 0);
    UT_ASSERT(os_strcasecmp(str, l_str) == 0);
    UT_ASSERT(os_strcasecmp(u_str, l_str) == 0);
}

UT_SECTION(test_strverscmp)

ut_test(equal_strings)
{
    UT_ASSERT(os_strverscmp("a", "a") == 0);
}

ut_test(human_ordered_strings)
{
    UT_ASSERT(os_strverscmp("sam99", "sam100") == -1);
}

#define BLANKS  "    \t\t   \t   "
#define TXT     "(some text)"

UT_SECTION(os_str_trim_left)

ut_test(trim_left_null_string_returns_null)
{
    UT_ASSERT(os_str_trim_left(NULL) == NULL);
}

ut_test(trim_left_empty_string_returns_empty)
{
    char s[] = "";
    char *r = os_str_trim_left(s);

    UT_ASSERT(r[0] == '\0');
}

ut_test(trim_left_only_blanks_returns_empty)
{
    char s[128] = BLANKS;
    char *r = os_str_trim_left(s);

    UT_ASSERT(r[0] == '\0');
}

ut_test(trim_left_no_blanks_returns_string_unchanged)
{
    char s[128] = TXT;
    char *r = os_str_trim_left(s);

    UT_ASSERT(strcmp(r, TXT) == 0);
}

ut_test(trim_left_with_blanks_removes_blanks)
{
    char s[128] = BLANKS TXT;
    char *r = os_str_trim_left(s);

    UT_ASSERT(strcmp(r, TXT) == 0);
}

UT_SECTION(os_str_trim_right)

ut_test(trim_right_null_string_returns_null)
{
    UT_ASSERT(os_str_trim_right(NULL) == NULL);
}

ut_test(trim_right_empty_string_returns_empty)
{
    char s[] = "";
    char *r = os_str_trim_right(s);

    UT_ASSERT(r[0] == '\0');
}

ut_test(trim_right_only_blanks_returns_empty)
{
    char s[128] = BLANKS;
    char *r = os_str_trim_right(s);

    UT_ASSERT(r[0] == '\0');
}

ut_test(trim_right_no_blanks_returns_string_unchanged)
{
    char s[128] = TXT;
    char *r = os_str_trim_right(s);

    UT_ASSERT(strcmp(r, TXT) == 0);
}

ut_test(trim_right_with_blanks_removes_blanks)
{
    char s[128] = TXT BLANKS;
    char *r = os_str_trim_right(s);

    UT_ASSERT(strcmp(r, TXT) == 0);
}

ut_test(trim_right_with_newline_returns_string_unchanged)
{
    char s[128] = TXT BLANKS "\n";
    char *r = os_str_trim_right(s);

    UT_ASSERT(strcmp(r, TXT BLANKS "\n") == 0);
}


ut_test(os_strlcpy)
{
    char s[17] = "0123456789ABCDEF";
    char r[17];
    char little[16];
    char very_little[10];

    UT_ASSERT_EQUAL(sizeof(s) - 1, os_strlcpy(r, s, sizeof(r)));
    UT_ASSERT_EQUAL(sizeof(s) - 1, os_strlcpy(little, s, sizeof(little)));
    UT_ASSERT_EQUAL(sizeof(s) - 1, os_strlcpy(very_little, s, sizeof(very_little)));
    UT_ASSERT_EQUAL('\0', little[sizeof(little) - 1]);
    UT_ASSERT_EQUAL('\0', very_little[sizeof(very_little) - 1]);
}
