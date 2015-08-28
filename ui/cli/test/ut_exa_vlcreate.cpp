/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "ui/cli/src/exa_vlcreate.h"
#include "ui/cli/test/command_ut_helper.h"

#ifdef WITH_BDEV
ut_test(command_default_short)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("-x"), const_cast<char*>("bdev"),
            const_cast<char*>("-s"), const_cast<char*>("10G"),
            const_cast<char*>("-r"), const_cast<char*>("10K")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlcreate>(argc, argv);
}

ut_test(command_default_long)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("bdev"),
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--readahead"), const_cast<char*>("10K")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlcreate>(argc, argv);
}

ut_test(access_option)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("bdev"),
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--access=private")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlcreate>(argc, argv);
}

ut_test(forceprivate_option)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("bdev"),
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--forceprivate")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlcreate>(argc, argv);
}

ut_test(lun_option_illegal_with_bdev)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("bdev"),
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--lun=1")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vlcreate>(argc, argv);
}
#endif

ut_test(command_specific_lum_ga_long)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
#ifdef WITH_BDEV
            const_cast<char*>("--export-method"), const_cast<char*>("iSCSI"),
#endif
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--lun"), const_cast<char*>("10"),
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlcreate>(argc, argv);
}


ut_test(command_specific_lum_ga_short)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
#ifdef WITH_BDEV
            const_cast<char*>("-x"), const_cast<char*>("iSCSI"),
#endif
            const_cast<char*>("-s"), const_cast<char*>("10G"),
            const_cast<char*>("-L"), const_cast<char*>("10"),
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_ok<exa_vlcreate>(argc, argv);
}

#ifdef WITH_BDEV
ut_test(access_option_illegal_with_iscsi)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("iSCSI"),
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--access=private")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vlcreate>(argc, argv);
}


ut_test(forceprivate_option_illegal_with_iscsi)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("iSCSI"),
            const_cast<char*>("--size"), const_cast<char*>("10G"),
            const_cast<char*>("--forceprivate")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vlcreate>(argc, argv);
}

ut_test(illegal_export_method)
{
    char* argv[] =
        {
            const_cast<char*>("exa_vlcreate"),
            const_cast<char*>("cl_test:dg_test:vl_test"),
            const_cast<char*>("--export-method"), const_cast<char*>("poh-poh"),
            const_cast<char*>("--size"), const_cast<char*>("10G")
        };
    int argc = sizeof(argv)/sizeof(char*);

    test_command_parsing_fail<exa_vlcreate>(argc, argv);
}
#endif
