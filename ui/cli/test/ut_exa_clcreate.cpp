/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "ui/cli/src/exa_clcreate.h"
#include "ui/cli/test/command_ut_helper.h"

using std::string;
using std::set;
using std::exception;

ut_test(command_default)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-i"),
            const_cast<char*>("test/1-8/:/dev/sdb"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_clcreate>(argc, argv);
}

ut_test(command_complicated_disks)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-i"),
            const_cast<char*>("'test/1-4/:/dev/sdb test5:/dev/sda6 test5:/dev/sdb test/7-8/:/dev/sdc test/7-8/:/dev/sdd'"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_clcreate>(argc, argv);
}

ut_test(command_missing_cluster_name)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-i"),
            const_cast<char*>("test/1-8/"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_clcreate>(argc, argv);
}

ut_test(command_missing_license)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-i"),
            const_cast<char*>("test/1-8/"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_clcreate>(argc, argv);
}

ut_test(command_missing_disks)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_clcreate>(argc, argv);
}

ut_test(command_with_config_file)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-c"),
            const_cast<char*>("config_file"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_clcreate>(argc, argv);
}

ut_test(command_datanetwork)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-i"),
            const_cast<char*>("test/1-8/"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("-D"),
            const_cast<char*>("'test1:172.16.75.1 test2:172.16.76.1'"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_clcreate>(argc, argv);
}

ut_test(command_spof)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("-i"),
            const_cast<char*>("test/1-8/"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("-s"),
            const_cast<char*>("[test/1-3 test4] [test5 test6][test/7:8/]"),
            const_cast<char*>("sam")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_clcreate>(argc, argv);
}

ut_test(command_spof_missing)
{
    char* argv[] =
        {
            const_cast<char*>("exa_clcreate"),
            const_cast<char*>("sam"),
            const_cast<char*>("-i"),
            const_cast<char*>("test/1-8/"),
            const_cast<char*>("-l"),
            const_cast<char*>("license.txt"),
            const_cast<char*>("-s")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_clcreate>(argc, argv);
}

ut_test(parse_spof)
{
    list<list<string> > spof_list;
    list<list<string> >::iterator spof_list_it;

    list<string> cur_spof;
    list<string>::iterator cur_spof_it;

    string cur_node;
    int i = 0;

    spof_list = exa_clcreate::parse_spof_groups("");
    UT_ASSERT(spof_list.empty());

    spof_list = exa_clcreate::parse_spof_groups(
            "  [test/1-3/  test4 ] \t [   test5 \ttest6 ]  [  test/7:8/] ");
    UT_ASSERT(!spof_list.empty());
    UT_ASSERT(spof_list.size() == 3);

    spof_list_it = spof_list.begin();

    cur_spof = *spof_list_it;
    UT_ASSERT(cur_spof.size() == 4);
    spof_list_it = spof_list.erase(spof_list_it);
    cur_spof_it = cur_spof.begin();
    for (i = 1; i <= 4; i++)
    {
        string str = "test";
        str += (i+'0');     /* disgracious hack. */
        cur_node = *cur_spof_it;
        UT_ASSERT(cur_node == str);
        cur_spof_it = cur_spof.erase(cur_spof_it);
    }

    cur_spof = *spof_list_it;
    UT_ASSERT(cur_spof.size() == 2);
    spof_list_it = spof_list.erase(spof_list_it);
    cur_spof_it = cur_spof.begin();
    for (i = 5; i <= 6; i++)
    {
        string str = "test";
        str += (i+'0');     /* disgracious hack. */
        cur_node = *cur_spof_it;
        UT_ASSERT(cur_node == str);
        cur_spof_it = cur_spof.erase(cur_spof_it);
    }

    cur_spof = *spof_list_it;
    UT_ASSERT(cur_spof.size() == 2);
    spof_list_it = spof_list.erase(spof_list_it);
    cur_spof_it = cur_spof.begin();
    for (i = 7; i <= 8; i++)
    {
        string str = "test";
        str += (i+'0');     /* disgracious hack. */
        cur_node = *cur_spof_it;
        UT_ASSERT(cur_node == str);
        cur_spof_it = cur_spof.erase(cur_spof_it);
    }

    UT_ASSERT(spof_list.begin() == spof_list.end());
}

static bool parse_broken_spof(const char *spof, const char *expected_error)
{
    list<list<string> > spof_list;

    try
    {
        spof_list = exa_clcreate::parse_spof_groups(spof);
        /* if we get there, we didn't get an exception */
        return false;
    }
    catch (exception &exc)
    {
        std::string msg(exc.what());
        /* Verify that the exception message is as expected */
        if (msg != expected_error)
            return false;
    }

    /* parsing failed correctly */
    return true;
}

ut_test(parse_spof_with_syntax_errors)
{

    UT_ASSERT(parse_broken_spof("[]",
                "Parse error in SPOF groups '[]'."));
    UT_ASSERT(parse_broken_spof("[test1][",
                "Parse error in SPOF groups '[test1]['."));
    UT_ASSERT(parse_broken_spof("[test1][]",
                "Parse error in SPOF groups '[test1][]'."));
    UT_ASSERT(parse_broken_spof("[test1][][test2]",
                "Parse error in SPOF groups '[test1][][test2]'."));
    UT_ASSERT(parse_broken_spof("][test1]",
                "Parse error in SPOF groups '][test1]'."));
    UT_ASSERT(parse_broken_spof("[[test1]",
                "Parse error in SPOF groups '[[test1]'."));
    UT_ASSERT(parse_broken_spof("[test1][[test2]",
                "Parse error in SPOF groups '[test1][[test2]'."));
    UT_ASSERT(parse_broken_spof("[test1][test2]]",
                "Parse error in SPOF groups '[test1][test2]]'."));
    UT_ASSERT(parse_broken_spof("[test1][test2]x",
                "Parse error in SPOF groups '[test1][test2]x'."));
    UT_ASSERT(parse_broken_spof("[test1]xx[test2]",
                "Parse error in SPOF groups '[test1]xx[test2]'."));
    UT_ASSERT(parse_broken_spof("xx[test1][test2]",
                "Parse error in SPOF groups 'xx[test1][test2]'."));
}
