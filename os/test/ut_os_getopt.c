/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_getopt.h"

#include <unit_testing.h>


UT_SECTION(os_getopt)

ut_test(os_getopt_without_anything_to_parse)
{
    char *const _argv[] = { "myself", NULL };
    int ret = os_getopt(1, _argv, "U:n:i:t:T:e:s:t");
    UT_ASSERT_EQUAL(-1, ret);
}

ut_test(os_getopt_without_dashoption_to_parse)
{
    char *const argv[] = { "myself", "badly passed argument", NULL };
    int ret = os_getopt(1, argv, "U:n:i:t:T:e:s:t");
    UT_ASSERT_EQUAL(-1, ret);
}

ut_test(os_getopt_not_in_list)
{
    char *const argv[] = { "myself", "-F",  NULL };

    int ret = os_getopt(2, argv, "U:n:i:t:T:e:s:t");
    UT_ASSERT_EQUAL('?', ret);
    UT_ASSERT_EQUAL(optopt, 'F');
}

ut_test(os_getopt_test_all)
{
    int sum = 0;
    int opt;
    char *const argv[] = { "myself", "-U", "-n", "-i", "-t", "-T", "-e", "-s",
	                   "-t", NULL };
    while ((opt = os_getopt(9, argv, "UnitTest")) != -1)
    {
	switch (opt) {
	    case 'U':
	    case 'n':
	    case 'i':
	    case 't':
	    case 'T':
	    case 'e':
	    case 's':
		//ut_printf("good value '%c'", (char) opt);
                sum+=opt;
		break;

	    default:
		ut_printf("bad value '%c'", (char) opt);
		UT_ASSERT(0);
	}
    }
    UT_ASSERT_EQUAL(sum, 832); /* 832 is the correct result for the sum,
				  I afford you :) */
}

ut_test(os_getopt_test_all_with_some_parm)
{
    int sum = 0;
    int opt;
    char *const argv[] = { "myself", "-U", "works", "-n", "-i", "-t",
	                   "-T", "greatly", "-e", "-s", "-t", NULL };
    while ((opt = os_getopt(11, argv, "U:nitT:est")) != -1)
    {
	switch (opt) {
	    case 'U':
                UT_ASSERT(!strcmp("works", optarg));
                sum += opt;
		break;

	    case 'T':
                UT_ASSERT(!strcmp("greatly", optarg));
                sum += opt;
		break;

	    case 'n':
	    case 'i':
	    case 't':
	    case 'e':
	    case 's':
		//ut_printf("good value '%c'", (char) opt);
                sum += opt;
		break;

	    default:
		ut_printf("bad value '%c'", (char) opt);
		UT_ASSERT(0);
	}
    }
    UT_ASSERT_EQUAL(sum, 832); /* 832 is the correct result for the sum,
				  I afford you :) */
}

UT_SECTION(os_getopt_long)

static struct option long_options[] = {
    {"Unit", 0, 0, 'U'},
    {"nit ", 0, 0, 'n'},
    {"it"  , 0, 0, 'i'},
    {"Test", 0, 0, 'T'},
    {"est" , 0, 0, 'e'},
    {"st"  , 0, 0, 's'},
    {"t"   , 0, 0, 't'},
    {0     , 0, 0,  0}
};

static int option_index;

ut_test(os_getopt_long_without_anything_to_parse)
{
    char *const argv[] = { "myself", NULL };

    int ret = os_getopt_long(1, argv, "U:n:i:t:T:e:s:t", long_options, &option_index);
    UT_ASSERT_EQUAL(-1, ret);
}

ut_test(os_getopt_long_without_dashoption_to_parse)
{
    char *const argv[] = { "myself", "badly passed argument", NULL };
    int ret = os_getopt_long(1, argv, "U:n:itT:e:s:t", long_options, &option_index);
    UT_ASSERT_EQUAL(-1, ret);
}

ut_test(os_getopt_long_not_in_list)
{
    char *const argv[] = { "myself", "-F",  NULL };

    int ret = os_getopt_long(2, argv, "U:n:itT:e:s:t", long_options, &option_index);
    UT_ASSERT_EQUAL('?', ret);
    UT_ASSERT_EQUAL(optopt, 'F');
}


ut_test(os_getopt_long_test_all)
{
    int sum = 0;
    int opt;
    char *const argv[] = { "myself", "--Unit", "-n", "--it", "-t", "-T", "--est",
	                   "-s", "-t", NULL };
    while ((opt = os_getopt_long(9, argv, "UnitTest",
		                 long_options, &option_index)) != -1)
    {
	switch (opt) {
	    case 'U':
	    case 'n':
	    case 'i':
	    case 't':
	    case 'T':
	    case 'e':
	    case 's':
		//ut_printf("good value '%c'", (char) opt);
                sum+=opt;
		break;

	    default:
		ut_printf("bad value '%c'", (char) opt);
		UT_ASSERT(0);
	}
    }
    UT_ASSERT_EQUAL(sum, 832); /* 832 is the correct result for the sum,
				  I afford you :) */
}

ut_test(os_getopt_long_test_all_with_some_params)
{
    static struct option long_options_param[] = {
	{"Unit", 1, 0, 'U'},
	{"nit ", 0, 0, 'n'},
	{"it"  , 0, 0, 'i'},
	{"Test", 0, 0, 'T'},
	{"est" , 1, 0, 'e'},
	{"st"  , 0, 0, 's'},
	{"t"   , 0, 0, 't'},
	{0     , 0, 0,  0}
    };


    int sum = 0;
    int opt;
    char *const argv[] = { "myself", "--Unit", "chuck", "-n", "--it", "-t", "-T",
	                   "--est", "Norris", "-s", "-t", NULL };
    while ((opt = os_getopt_long(11, argv, "UnitTest",
		                 long_options_param, &option_index)) != -1)
    {
	switch (opt) {
	    case 'U':
	    case 'n':
	    case 'i':
	    case 't':
	    case 'T':
	    case 'e':
	    case 's':
		//ut_printf("good value '%c'", (char) opt);
                sum+=opt;
		break;

	    default:
		ut_printf("bad value '%c'", (char) opt);
		UT_ASSERT(0);
	}
    }
    UT_ASSERT_EQUAL(sum, 832); /* 832 is the correct result for the sum,
				  I afford you :) */
}
