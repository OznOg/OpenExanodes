/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifdef WIN32
#include "Windows.h"
#else
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <unit_testing.h>

#include "os/include/os_system.h"
#include "os/include/os_stdio.h"

#ifdef WIN32

static void exit_test(int sent, int expected)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char cmd[256];
    DWORD status;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    os_snprintf(cmd, sizeof(cmd), "os_system_helper %d", sent);

    UT_ASSERT(CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    UT_ASSERT_EQUAL(WAIT_OBJECT_0, WaitForSingleObject(pi.hProcess, 1000));
    UT_ASSERT(GetExitCodeProcess(pi.hProcess, &status));
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    UT_ASSERT_EQUAL(expected, status);
}

#else /* WIN32 */

static void exit_test(int sent, int expected)
{
    char sent_str[16];
    int status;
    pid_t pid;

    os_snprintf(sent_str, sizeof(sent_str), "%d", sent);

    pid = fork();

    if (pid == 0)
        execlp("./os_system_helper", "os_system_helper", sent_str, (char *)NULL);

    waitpid(pid, &status, 0);

    UT_ASSERT(WIFEXITED(status));
    UT_ASSERT_EQUAL(expected, WEXITSTATUS(status));
}

#endif /* WIN32 */

UT_SECTION(test_exit_values)

ut_test(exit_neg)
{
    exit_test(-1, 255);
}

ut_test(exit_0)
{
    exit_test(0, 0);
}

ut_test(exit_13)
{
    exit_test(13, 13);
}

ut_test(exit_255)
{
    exit_test(255, 255);
}

ut_test(exit_256)
{
    exit_test(256, 0);
}

ut_test(exit_65530)
{
    exit_test(65530, 250);
}

UT_SECTION(Check_the_return_code_from_the_command_is_retrieved)

ut_test(correct_command)
{
    char *const ls_ok[] = {
#ifdef WIN32
	"ipconfig", "/all", NULL };
#else
	"ls", "-a" , "-l", NULL };
#endif
    UT_ASSERT_EQUAL(0, os_system(ls_ok));
}

ut_test(command_returns_an_error)
{
    char *const ls_error[] = {
#ifdef WIN32
	"dir", "-gni" , NULL };
#else
	"ls", "-P" , NULL };
#endif
    UT_ASSERT(os_system(ls_error) != 0);
}

UT_SECTION(Parameter_lists)

ut_test(command_takes_a_huge_number_of_parameter)
{
    char *const many_args[] = {
#ifdef WIN32
	"cmd", "/C", "echo",
#else
	"echo",
#endif
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	"test", "test", "test", "test", "test", "test", "test", "test","test",
	NULL };
    UT_ASSERT_EQUAL(0, os_system(many_args));
}

ut_test(command_has_one_very_long_param)
{
    char *const long_arg[] = {
#ifdef WIN32
	"cmd", "/C", "echo",
	"Linux is built for the network more than printing"
	"When Windows was first built, it was mostly a paper world. One of "
	"the beautiful things about Windows was that everything you did was"
	"pretty to look at and easy to print. This beginning has affected "
	"the evolution of Windows.",
#else
	"ls",
	"/../../../../../../../../../../../../../../"
	    "../../../../../../../../../../../../../../.."
	    "/../../../../../../../../../../../../../../"
	    "/../../../../../../../../../../../../../../"
	    "/../../../../../../../../../../../../../../"
	    "/../../../../../../../../../../../../../../"
	    "/../../../../../../../../../../../../../../"
	    "../../../../../../../../../../../../../",
#endif
	NULL };
    UT_ASSERT_EQUAL(0, os_system(long_arg));
}

UT_SECTION(Command_is_not_valid)

ut_test(command_does_not_exist)
{
    char *const bad_name[] = { "./bad_name", NULL };
    UT_ASSERT_EQUAL(-ENOENT, os_system(bad_name));
}

/* Not relevant on windows */
ut_test(file_is_exec_but_not_a_regular_file)
{
#ifdef WIN32
    char *const not_a_regfile[] = { "c:\\windows", NULL };
    UT_ASSERT_EQUAL(-EPERM, os_system(not_a_regfile));
#else
    char *const not_a_regfile[] = { "/tmp/", NULL };
    UT_ASSERT_EQUAL(-EACCES, os_system(not_a_regfile));
#endif
}

ut_test(file_is_not_an_exec)
{
#ifdef WIN32
    char *const not_exec[] = { "c:\\windows\\system.ini", NULL };
    UT_ASSERT(os_system(not_exec) != 0);
#else
    char *const not_exec[] = { "/etc/fstab", NULL };
    UT_ASSERT_EQUAL(-EACCES, os_system(not_exec));
#endif
}

ut_test(pass_null_array)
{
    char *const null[] = { NULL };
    UT_ASSERT_EQUAL(-EINVAL, os_system(null));
}

