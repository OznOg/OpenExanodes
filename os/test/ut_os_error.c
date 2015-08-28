/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "os/include/os_error.h"


ut_test(os_strerror_errno)
{
    const char *msg = os_strerror(EINVAL);

    UT_ASSERT(strcmp(msg, "Invalid argument") == 0);
}


#ifdef WIN32

ut_test(WinErroToError_errno)
{
    int error = os_error_from_win(WSAEINVAL);

    UT_ASSERT(error == EINVAL);
}

ut_test(WinErroToError_windows)
{
    int error = os_error_from_win(ERROR_INVALID_HANDLE);

    UT_ASSERT(error == (ERROR_INVALID_HANDLE | OS_ERROR_WINDOWS_BIT));
    UT_ASSERT((error & 0xFFFF) == ERROR_INVALID_HANDLE);
}

ut_test(os_strerror_windows)
{
    const char *msg = os_strerror(os_error_from_win(ERROR_INVALID_HANDLE));

    UT_ASSERT(strcmp(msg, "The handle is invalid") == 0);
}

#else /* WIN32 */

#include <netdb.h>

ut_test(os_error_from_gai)
{
    int error0 = os_error_from_gai(0, 42);
    int error1 = os_error_from_gai(EAI_NONAME, 0);
    int error2 = os_error_from_gai(EAI_SYSTEM, ENOMEM);

    UT_ASSERT(error0 == 0);
    UT_ASSERT(error1 == (-EAI_NONAME | OS_ERROR_GAI_BIT));
    UT_ASSERT(error2 == ENOMEM);
}

ut_test(os_strerror_gai)
{
    const char *msg = os_strerror(os_error_from_gai(EAI_NONAME, 0));

    UT_ASSERT(strcmp(msg, "Name or service not known") == 0);
}

#endif /* WIN32 */
