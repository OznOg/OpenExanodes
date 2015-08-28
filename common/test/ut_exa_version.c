/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "unit_testing.h"

#include "common/include/exa_error.h"
#include "common/include/exa_version.h"

#include "os/include/strlcpy.h"

typedef struct {
    const char *version;
    bool is_major;
    const char *major;
} version_case_t;

static const version_case_t version_cases[] = {
    { "3.0",     true,  "3.0" },
    { "3.0.1",   false, "3.0" },
    { "2.5",     true,  "2.5" },
    { "2.5.1.2", false, "2.5" },
    { "5",       false, NULL },
    { "22",      false, NULL },
    { NULL,      false, NULL }
};

/* These are for testing exa_version_is_equal*/
typedef struct {
    const char *version1;
    bool is_equal;
    const char *version2;
} version_eq_t;

static const version_eq_t version_equality_test[] = {
    { "3.0",     true,   "3.0"     },
    { "3.1",     true,   "3.1"     },
    { "2.1",     true,   "2.1"     },
    { "3.0.0",   true,   "3.0.0"   },
    { "3.0.0",   false,  "2.0.0"   },
    { "3.1.0",   false,  "3.0.0"   },
    { "1.2.3.4", false,  "1.2.3.3" },
    { NULL,      false,  NULL      }
};


UT_SECTION(exa_version_is_major)

ut_test(is_major_null_return_false)
{
    UT_ASSERT(!exa_version_is_major(NULL));
}

ut_test(is_major_several_cases)
{
    int i;

    for (i = 0; version_cases[i].version != NULL; i++)
    {
        bool is_maj = exa_version_is_major(version_cases[i].version);
        UT_ASSERT_VERBOSE(is_maj == version_cases[i].is_major,
                          "is_major(%s) failed", version_cases[i].version);
    }
}

UT_SECTION(exa_version_get_major)

ut_test(get_major_null_null_returns_false)
{
    UT_ASSERT(!exa_version_get_major(NULL, NULL));
}

ut_test(get_major_of_null_version_returns_false)
{
    exa_version_t major;

    UT_ASSERT(!exa_version_get_major(NULL, major));
}

ut_test(get_major_null_result_buffer_returns_false)
{
    UT_ASSERT(!exa_version_get_major("3.0.1", NULL));
}

ut_test(get_major_several_cases)
{
    int i;

    for (i = 0; version_cases[i].version != NULL; i++)
    {
        exa_version_t major;
        bool ok;

        ok = exa_version_get_major(version_cases[i].version, major);
        if (version_cases[i].major == NULL)
            UT_ASSERT(!ok);
        else
        {
            UT_ASSERT(ok);
            UT_ASSERT_EQUAL_STR(version_cases[i].major, major);
        }
    }
}

/* exa_version_from_str must be unit-tested before exa_version_is_equal and
 * exa_version_copy
 * FIXME: we should create exa_version_to_str and check that applying sequentially
 * both functions leaves unchanged the data.
 */
UT_SECTION(exa_version_from_str)

ut_test(too_long_string_returns_EXA_ERR_VERSION)
{
    exa_version_t a;
    char src[EXA_VERSION_LEN + 100];
    memset(src, '9', EXA_VERSION_LEN + 100);
    /* to have a well-formed string, we create a version like 9.999(...)9 which
     * is valid but too long
    */
    src[1] = '.';
    src[EXA_VERSION_LEN + 99] = '\0';
    UT_ASSERT_EQUAL(-EXA_ERR_VERSION, exa_version_from_str(a, src));
}

ut_test(correctly_sized_string_returns_EXA_SUCCESS)
{
    exa_version_t a;
    UT_ASSERT_EQUAL(EXA_SUCCESS, exa_version_from_str(a, "3.0.0"));
}

UT_SECTION(exa_version_is_equal)

ut_test(exa_version_is_equal_several_cases)
{
    int i;

    for (i = 0; version_equality_test[i].version1 != NULL; i++)
    {
        exa_version_t a, b;
        bool is_equal = version_equality_test[i].is_equal;

        exa_version_from_str(a, version_equality_test[i].version1);
        exa_version_from_str(b, version_equality_test[i].version2);

        UT_ASSERT_VERBOSE(is_equal == exa_version_is_equal(a, b),
                          "is_equal(%s, %s) failed",
                          version_equality_test[i].version1,
                          version_equality_test[i].version2);
    }
}

UT_SECTION(exa_version_copy)

ut_test(copy_works)
{
    exa_version_t a, b;
    exa_version_from_str(a, "3.0.0");
    exa_version_copy(b,a);
    UT_ASSERT(exa_version_is_equal(a,b));
}
