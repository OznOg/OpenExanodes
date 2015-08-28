/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "ui/cli/src/exa_vlstart.h"
#include "ui/cli/test/command_ut_helper.h"


ut_test(command_default)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlstart"),
            const_cast<char*>("-a"),
            const_cast<char*>("cl_test:dg_test:vl_test")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlstart>(argc, argv);
}

ut_test(command_some_nodes)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlstart"),
            const_cast<char*>("-n sam11"),
            const_cast<char*>("cl_test:dg_test:vl_test")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlstart>(argc, argv);
}

ut_test(command_no_nodes)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlstart"),
            const_cast<char*>("cl_test:dg_test:vl_test")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vlstart>(argc, argv);
}
