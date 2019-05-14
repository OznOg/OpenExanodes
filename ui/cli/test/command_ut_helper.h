/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __COMMAND_UT_HELPER_H__
#define __COMMAND_UT_HELPER_H__

#include <string>
#include <sstream>


template<class T> static void
test_command_parsing_ok(int argc, char *argv[])
{
    std::ostringstream to_display;
    for (int i = 0; i < argc; i++)
        to_display << " " << const_cast<const char*>(argv[i]);

    T command;

    try
    {
        command.parse(argc, argv);
        return;
    }
    catch (CommandException& ex)
    {
        ut_printf("Unexpected CommandException. %s", ex.what());
    }
    catch (...)
    {
        // Do nothing
    }

    ut_printf("COMMAND FAILED: %s", to_display.str().c_str());
    UT_FAIL();
}


template<class T> static void
test_command_parsing_fail(int argc, char *argv[])
{
    std::ostringstream to_display;
    for (int i = 0; i < argc; i++)
        to_display << " " << const_cast<const char*>(argv[i]);

    T command;

    try
    {
        command.parse(argc, argv);
    }
    catch (CommandException& ex)
    {
        ut_printf("Expected CommandException. %s", ex.what());
        return;
    }
    catch (...)
    {
        // Do nothing
    }

    ut_printf("COMMAND DIDNOT FAIL: %s", to_display.str().c_str());
    UT_FAIL();
}

#endif /* __COMMAND_UT_HELPER_H__ */

