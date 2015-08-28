/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "ui/cli/src/exa_vltune.h"
#include "ui/cli/test/command_ut_helper.h"


ut_test(parameter_readahead)
{
    char *argv[] =
        {
            const_cast<char*>("exa_vltune"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("readahead=8M")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vltune>(argc, argv);
}

ut_test(parameter_lun)
{
    char *argv[] =
        {
            const_cast<char*>("exa_vltune"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("lun=213")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vltune>(argc, argv);
}


ut_test(option_missing)
{
    char *argv[] =
        {
            const_cast<char*>("exa_vltune"),
            const_cast<char*>("cl_test:dg_test:vl_test")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vltune>(argc, argv);
}


ut_test(bad_parameter_format_ra)
{
    char *argv[] =
        {
            const_cast<char*>("exa_vltune"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("readahead-8M")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vltune>(argc, argv);
}

ut_test(bad_parameter_format_lun)
{
    char *argv[] =
        {
            const_cast<char*>("exa_vltune"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("lun-213")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vltune>(argc, argv);
}

ut_test(param_list_option)
{
    char *argv[] =
        {
            const_cast<char*>("exa_vltune"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--list")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vltune>(argc, argv);
}


