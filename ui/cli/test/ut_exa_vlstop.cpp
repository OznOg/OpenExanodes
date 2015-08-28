/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "ui/cli/src/exa_vlstop.h"
#include "ui/cli/test/command_ut_helper.h"


ut_test(command_default)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlstop"),
            const_cast<char*>("-a"),
            const_cast<char*>("cl_test:dg_test:vl_test")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlstop>(argc, argv);
}

ut_test(command_iscsi)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlstop"),
            const_cast<char*>("-a"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlstop>(argc, argv);
}

ut_test(command_iscsi_abbreviated)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlstop"),
            const_cast<char*>("-a"),
            const_cast<char*>("cl_test:dg_test:vl_test")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlstop>(argc, argv);
}
