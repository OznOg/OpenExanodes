/*
 * Copyright 2002, 2015 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes Unit test library and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h> /* for ut_printf() */
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#endif

#ifdef __cplusplus
#include <iostream>
#include <stdexcept>
#endif

#ifdef WIN32
#  ifndef __cplusplus
#    ifndef bool
#      define bool  _Bool
#      define true  1
#      define false 0
#    endif
#  endif
#  define _ut_open _open
#  define _ut_close _close
#  define _ut_fileno _fileno
#  define _ut_dup _dup
#  define _ut_dup2 _dup2
#  define _ut_putenv _putenv
/* Windows 'ligth' reimplementation of linux' snprintf.
 * This juste make sure there is a trailing '\0' , it DOES NOT return the same
 * value than linux would return (the amount of byte that 'would have been
 * written). Anyway, this feature is useless here */
static int _ut_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    if (str && size > 0)
	str[size - 1] = '\0';

    return _vsnprintf(str, size, format, ap);
}

static int _ut_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list argv;
    int res;
    FILE* file;

    va_start(argv, format);
    res = _ut_vsnprintf(str, size, format, argv);
    va_end(argv);

    return res;
}

#  define DEV_NULL "nul"
#else /* WIN32 */
#  define _ut_open open
#  define _ut_close close
#  define _ut_fileno fileno
#  define _ut_dup dup
#  define _ut_dup2 dup2
#  define _ut_putenv putenv
#  define _ut_snprintf snprintf
#  define color_vprintf(format, ...) vprintf(format, ## __VA_ARGS__)
#  define color_printf(format, ...) printf(format, ## __VA_ARGS__)
#  define DEV_NULL "/dev/null"
#endif /* WIN32 */

#define COLOR_TITLE   "\033[1;34m"
#define COLOR_SUCCESS "\033[1;32m"
#define COLOR_FAILURE "\033[1;31m"
#define COLOR_SKIP    "\033[1;33m"
#define COLOR_NORMAL  "\033[0;39m"

/** Sets up any data a section needs */
typedef void (*setup_fn_t)(void);

/** Performs a single test within a section */
typedef void (*test_fn_t)(void);

/** Cleans up the data allocated for a section */
typedef void (*cleanup_fn_t)(void);

/** A test */
typedef struct test
{
    int index;                /**< Test index */
    const char *name;         /**< Name of the test */
    test_fn_t run;            /**< Actual test function */
    bool lengthy;             /**< Whether the test may take a long time */
    unsigned timeout;         /**< Timeout in seconds (0 = none) */
    int expected_signal;      /**< Signal supposed to be raised (0 = none) */
} test_t;

/** A section */
typedef struct section
{
    const char *name;              /**< Name of the section */
    setup_fn_t setup;              /**< Setup of data */
    cleanup_fn_t cleanup;          /**< Cleanup of data */
    int nb_tests;                  /**< Number of tests in suite */
    test_t *tests;                 /**< The test cases */
} section_t;

typedef enum
{
    __UT_RESULT_FAILED,
    __UT_RESULT_PASSED,
    __UT_RESULT_SKIPPED
} ut_result_t;

#define __UT_FAILED_STR COLOR_FAILURE "*FAILED*" COLOR_NORMAL
#define __UT_PASSED_STR COLOR_SUCCESS "PASSED" COLOR_NORMAL
#define __UT_SKIPPED_STR COLOR_SKIP "SKIPPED" COLOR_NORMAL

typedef enum { __UT_IN_SETUP, __UT_IN_CLEANUP, __UT_IN_TEST } ut_in_t;

static ut_in_t __ut_currently_in;
static int __ut_test_index = 0;
static int __ut_must_fork = 1;
static ut_result_t __ut_test_result = __UT_RESULT_PASSED;
static char __ut_test_report_str[512];
static int __ut_pass_count = 0;
static int __ut_skip_count = 0;
static int __ut_section_count = 0;
static int __ut_test_count = 0;
static bool __ut_shuffle_tests = false;
static bool __ut_test_verbose = false;
static bool __ut_test_quiet = false;
static bool __ut_skip_lengthy = false;
static bool __ut_output_xml = false;
static char *__ut_argv0;

static section_t sections[128];

/* forward declarations; definitions are injected by ut_build */
static void ut_register_all(void);
static void ut_unregister_all(void);

static const char *ut_result_str(ut_result_t result)
{
    switch (result)
    {
    case __UT_RESULT_FAILED: return __UT_FAILED_STR;
    case __UT_RESULT_PASSED: return __UT_PASSED_STR;
    case __UT_RESULT_SKIPPED: return __UT_SKIPPED_STR;
    }

    abort();

    return NULL;
}

static void ut_end(ut_result_t result)
{
    if (__ut_must_fork)
        exit(result == __UT_RESULT_PASSED ? 0 : 1);
    else
        __ut_test_result = result;
}

#ifdef WIN32
static void color_vprintf(const char *format, va_list al)
{
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char *escape_char = 0;
    char buf[1024];
    char *pos = buf;

    _ut_vsnprintf(buf, sizeof(buf), format, al);

    while ((escape_char = strchr(pos, '\033')) != NULL)
    {
        WORD color;

        *escape_char = '\0';
        printf(pos);

        *escape_char = '\033';
        if (strncmp(escape_char, COLOR_TITLE, strlen(COLOR_TITLE)) == 0)
            color = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        else if (strncmp(escape_char, COLOR_SUCCESS, strlen(COLOR_SUCCESS)) == 0)
            color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        else if (strncmp(escape_char, COLOR_FAILURE, strlen(COLOR_FAILURE)) == 0)
            color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        else if (strncmp(escape_char, COLOR_SKIP, strlen(COLOR_SKIP)) == 0)
            color = FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        else if (strncmp(escape_char, COLOR_NORMAL, strlen(COLOR_NORMAL)) == 0)
            color = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE;
	else
	    /* Color was not recognised... probably escape chars from user
	     * program -> In this case, it is just ignored... */
	    color = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE;

        SetConsoleTextAttribute(hStdOut, color);

        pos = strchr(escape_char, 'm');
        assert(buf != NULL);
        pos++;
    }

    printf(pos);
}

static void color_printf(const char *format, ...)
{
    va_list al;

    va_start(al, format);
    color_vprintf(format, al);
    va_end(al);
}
#endif

/* -- plain test output -- */

static void ut_printf_al_plain(const char *where, const char *format, va_list al)
{
    switch (__ut_currently_in)
    {
    case __UT_IN_SETUP:
        color_printf("       SETUP: ");
        break;

    case __UT_IN_CLEANUP:
        color_printf("     CLEANUP: ");
        break;

    case __UT_IN_TEST:
        color_printf("    %s %3d: ", where, __ut_test_index + 1);
    }
    color_vprintf(format, al);
    color_printf("\n");
}

static void ut_print_report_beginning_plain(const char *ut_program)
{
    color_printf(COLOR_TITLE "====== TEST SUITE '%s' ======" COLOR_NORMAL "\n",
           ut_program);
}

static void ut_print_report_end_plain(void)
{
    bool success = (__ut_pass_count + __ut_skip_count == __ut_test_count);

    color_printf("%s", success ? COLOR_SUCCESS : COLOR_FAILURE);
    color_printf("    RESULT: %d tests, %d passed, %d skipped, %d failed%s\n",
           __ut_test_count,
           __ut_pass_count,
           __ut_skip_count,
           __ut_test_count - (__ut_pass_count + __ut_skip_count),
           COLOR_NORMAL);
}

static void ut_print_section_beginning_plain(const char *testsuite_name)
{
    if (strlen(testsuite_name) == 0)
	return;

    color_printf("%sSECTION '%s'%s\n", COLOR_TITLE, testsuite_name, COLOR_NORMAL);
}

static void ut_print_section_end_plain(void)
{
}

static void ut_print_test_beginning_plain(const test_t *test)
{
    char begin_str[128];
    unsigned attr_count = 0;

    strcpy(begin_str, "(");

    /* XXX strcat() is unsafe but it will do for now since we only have
     * three attributes to consider */
    if (!__ut_must_fork)
    {
        strcat(begin_str, "notforked");
        attr_count++;
    }

    if (test->lengthy)
    {
        if (attr_count > 0)
            strcat(begin_str, ", ");
        strcat(begin_str, "lengthy");
        attr_count++;
    }

    if (test->timeout > 0)
    {
        char timeout_str[32];

        _ut_snprintf(timeout_str, sizeof(timeout_str), "%us timeout",
                 test->timeout);
        if (attr_count > 0)
            strcat(begin_str, ", ");
        strcat(begin_str, timeout_str);
        attr_count++;
    }

    if (test->expected_signal > 0)
    {
        char signal_str[32];

        _ut_snprintf(signal_str, sizeof(signal_str), "signal %d expected",
                     test->expected_signal);
        if (attr_count > 0)
            strcat(begin_str, ", ");
        strcat(begin_str, signal_str);
        attr_count++;
    }

    strcat(begin_str, ")");

    if (attr_count > 0)
        ut_printf(__ut_test_report_str, test->name, begin_str);
}

static void ut_print_test_end_plain(const test_t *test, ut_result_t result)
{
    ut_printf(__ut_test_report_str, test->name, ut_result_str(result));
}

/* -- xml output -- */

static void ut_printf_al_xml(const char *where, const char *format, va_list al)
{
    switch (__ut_currently_in)
    {
    case __UT_IN_SETUP:
        printf("<output stage='setup' from='%s'><![CDATA[", where);
        vprintf(format, al);
        printf("]]></output>\n");
        break;

    case __UT_IN_CLEANUP:
        printf("<output stage='cleanup' from='%s'><![CDATA[", where);
        vprintf(format, al);
        printf("]]></output>\n");
        break;

    case __UT_IN_TEST:
        printf("<output stage='exec' from='%s'><![CDATA[", where);
        vprintf(format, al);
        printf("]]></output>\n");
    }
}

static void ut_print_report_beginning_xml(const char *ut_program)
{
    printf("<?xml version='1.0' encoding='UTF-8'?>\n");
    printf("<report program='%s'>\n", ut_program);

}

static void ut_print_report_end_xml(void)
{
    printf("</report>\n");
}

static void ut_print_section_beginning_xml(const char *testsuite_name)
{
    printf("<section");
    if (strlen(testsuite_name) != 0)
        printf(" name='%s'",  testsuite_name);
    printf(">\n");
}

static void ut_print_section_end_xml(void)
{
    printf("</section>\n");
}

static void ut_print_test_beginning_xml(const test_t *test)
{
    printf("<test name='%s' index='%d'", test->name, test->index + 1);
    if (! __ut_must_fork)
        printf(" notforked='true'");
    if (test->lengthy)
        printf(" lengthy='true'");
    if (test->timeout > 0)
        printf(" timeout='%us'", test->timeout);
    if (test->expected_signal > 0)
        printf(" expected_signal='%d'", test->expected_signal);
    printf(">\n");
}

static void ut_print_test_end_xml(const test_t *test, ut_result_t result)
{
    /* Suppress the warning about unused parameter */
    (void)test;

    printf("<result>%d</result>\n", result);
    printf("</test>\n");

}

void (*ut_printf_al)(const char *where, const char *format, va_list al)
    = ut_printf_al_plain;
void (*ut_print_report_beginning)(const char *ut_program)
    = ut_print_report_beginning_plain;
void (*ut_print_report_end)(void)
    = ut_print_report_end_plain;
void (*ut_print_section_beginning)(const char *section_name)
    = ut_print_section_beginning_plain;
void (*ut_print_section_end)(void)
    = ut_print_section_end_plain;
void (*ut_print_test_beginning)(const test_t *test)
    = ut_print_test_beginning_plain;
void (*ut_print_test_end)(const test_t *test, ut_result_t result)
    = ut_print_test_end_plain;

static void ut_set_output_xml(void)
{
    ut_printf_al = ut_printf_al_xml;

    ut_print_report_beginning = ut_print_report_beginning_xml;
    ut_print_report_end = ut_print_report_end_xml;

    ut_print_section_beginning = ut_print_section_beginning_xml;
    ut_print_section_end = ut_print_section_end_xml;

    ut_print_test_beginning = ut_print_test_beginning_xml;
    ut_print_test_end = ut_print_test_end_xml;
}

void ut_printf(const char *format, ...)
{
    va_list al;

    va_start(al, format);
    ut_printf_al("TEST", format, al);
    va_end(al);
}

void ut_code_printf(const char *format, ...)
{
    va_list al;
    if (!__ut_test_verbose)
	return;

    va_start(al, format);
    ut_printf_al("CODE", format, al);
    va_end(al);
}

static void ut_run_setup(setup_fn_t setup_fn)
{
    __ut_currently_in = __UT_IN_SETUP;
    setup_fn();
}

static void ut_run_cleanup(cleanup_fn_t cleanup_fn)
{
    __ut_currently_in = __UT_IN_CLEANUP;
    cleanup_fn();
}

static void ut_do_test(test_t *test)
{
    int log = -1, saved_stdout = -1, saved_stderr = -1;

    /* Avoid breaking XML with testee's output */
    if (__ut_output_xml)
    {
	log = _ut_open(DEV_NULL, O_WRONLY);
        if (log < 0)
            return;

        saved_stdout = _ut_dup(_ut_fileno(stdout));
        if (saved_stdout < 0)
            goto done;

        saved_stderr = _ut_dup(_ut_fileno(stderr));
        if (saved_stderr < 0)
            goto done;

        if (_ut_dup2(log, _ut_fileno(stdout)) < 0 || _ut_dup2(log, _ut_fileno(stderr)) < 0)
	    goto done;
    }

    test->run();

done:
    if (__ut_output_xml)
    {
        _ut_close(log);
        if (saved_stdout >= 0)
        {
            _ut_dup2(saved_stdout, _ut_fileno(stdout));
	    _ut_close(saved_stdout);
        }
        if (saved_stderr >= 0)
        {
            _ut_dup2(saved_stderr, _ut_fileno(stderr));
            _ut_close(saved_stderr);
        }
    }
}

static void ut_run_test(section_t *suite, test_t *test)
{
    __ut_test_index = test->index;

    if (test->lengthy && __ut_skip_lengthy)
    {
        __ut_test_result = __UT_RESULT_SKIPPED;
        if (! __ut_test_quiet)
            ut_print_test_beginning(test);
        goto done;
    }

    if (!__ut_must_fork)
    {
        if (suite->setup != NULL)
            ut_run_setup(suite->setup);
#ifdef __cplusplus
        try
        {
#endif
        __ut_currently_in = __UT_IN_TEST;
        __ut_test_result = __UT_RESULT_PASSED;
        if (! __ut_test_quiet)
            ut_print_test_beginning(test);
        ut_do_test(test);
#ifdef __cplusplus
    }
    catch(std::exception & exc) {
        std::cout << "Exception: " << exc.what() << std::endl;
        ut_end(__UT_RESULT_FAILED);
    }
#endif
        if (suite->cleanup != NULL)
            ut_run_cleanup(suite->cleanup);
    }
    else
    {

#ifdef WIN32

        char cmdline[256];
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        DWORD ret;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        if (! __ut_test_quiet)
            ut_print_test_beginning(test);

        _ut_snprintf(cmdline, sizeof(cmdline), "%s -d -q -n %d%s%s%s",
                  __ut_argv0, test->index + 1,
                  __ut_output_xml ? " -x" : "",
                  __ut_skip_lengthy ? " -s" : "",
                  __ut_test_verbose ? " -v" : "");

        if (! CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            ut_printf("Failed to create process");
            ut_end(__UT_RESULT_FAILED);
        }

        ret = WaitForSingleObject(pi.hProcess, test->timeout != 0 ? 1000 * test->timeout : INFINITE);
        if (ret != WAIT_OBJECT_0)
        {
            ut_printf("Test case timed out");
            __ut_test_result = __UT_RESULT_FAILED;
            CloseHandle(pi.hProcess);
            goto done;
        }

        if (! GetExitCodeProcess(pi.hProcess, &ret))
        {
            ut_printf("Failed to get exit code");
            __ut_test_result = __UT_RESULT_FAILED;
            CloseHandle(pi.hProcess);
            goto done;
        }

        CloseHandle(pi.hProcess);

        __ut_test_result = (ret == 0) ? __UT_RESULT_PASSED : __UT_RESULT_FAILED;

#else /* WIN32 */

        pid_t pid;
        int child_status;

        pid = fork();
        if (pid == 0)
        {
            if (suite->setup != NULL)
                ut_run_setup(suite->setup);
#ifdef __cplusplus
            try
            {
#endif
            __ut_currently_in = __UT_IN_TEST;
            if (! __ut_test_quiet)
                ut_print_test_beginning(test);
            ut_do_test(test);
#ifdef __cplusplus
        }
        catch(std::exception & exc) {
            std::cout << "Exception: " << exc.what() << std::endl;
            ut_end(__UT_RESULT_FAILED);
        } catch(...) {
            std::cout << "Unknown error" << std::endl;
            ut_end(__UT_RESULT_FAILED);
        }
#endif
            if (suite->cleanup != NULL)
                ut_run_cleanup(suite->cleanup);
            ut_end(__UT_RESULT_PASSED);
        }
        else if (pid > 0)
        {
            bool timed_out = false;
            pid_t died_pid = 0;

            if (test->timeout == 0)
                died_pid = wait(&child_status);
            else
            {
                unsigned long time_left = 1000000 * test->timeout;

                /* Polling: can't use SIGCHLD because the testee may have
                 * its own SIGCHLD handler */
                while (time_left > 0)
                {
                    died_pid = waitpid(pid, &child_status, WNOHANG);
                    if (died_pid > 0)
                        break;
                    /* 1/10th of a second */
                    usleep(100000);
                    time_left -= 100000;
                }
                timed_out = (died_pid <= 0);
            }

            if (timed_out)
            {
                int _status;

                kill(pid, SIGKILL);
                waitpid(pid, &_status, 0);

                ut_printf("Test case timed out");
                __ut_test_result = __UT_RESULT_FAILED;
            }
            else if (died_pid <= 0 || child_status != 0)
            {
                __ut_test_result = __UT_RESULT_FAILED;

                if (WIFSIGNALED(child_status))
                {
                    int sig = WTERMSIG(child_status);

                    if (test->expected_signal > 0)
                    {
                        if (sig == test->expected_signal)
                            __ut_test_result = __UT_RESULT_PASSED;
                        else
                            ut_printf("Expected signal %d, got %d",
                                      test->expected_signal, sig);
                    }
                    else
                        ut_printf("Test case died unexpectedly (signal %d).", sig);
                }
            }
            else
            {
                if (test->expected_signal > 0)
                {
                    ut_printf("Expected signal %d, got none", test->expected_signal);
                    __ut_test_result = __UT_RESULT_FAILED;
                }
                else
                    __ut_test_result = __UT_RESULT_PASSED;
            }
        }
        else
            ut_printf("Failed forking");

#endif /* WIN32 */

    }

done:
    __ut_currently_in = __UT_IN_TEST;
    if (! __ut_test_quiet)
        ut_print_test_end(test, __ut_test_result);

    switch (__ut_test_result)
    {
    case __UT_RESULT_FAILED:
        break;
    case __UT_RESULT_PASSED:
        __ut_pass_count++;
        break;
    case __UT_RESULT_SKIPPED:
        __ut_skip_count++;
        break;
    }
}

static void ut_random_init(void)
{
#ifdef WIN32
    /* XXX  Do the same on Linux? Why did we bother with /dev/urandom? */
    srand(time(NULL));
#else
    int ret;
    int fdrandom = -1;
    int ut_random_seed;

    fdrandom = _ut_open("/dev/urandom", O_RDONLY);
    ret = read(fdrandom, &ut_random_seed,
             sizeof(ut_random_seed));
    _ut_close(fdrandom);
    if (ret != sizeof(ut_random_seed))
    {
       color_printf("cannot initialize random number generator\n");
       abort();
    }
    srand(ut_random_seed);
#endif
}


static void ut_shuffle_suite(section_t *section)
{
    int nb_swaps = section->nb_tests * 2;
    test_t tmp_test;

    /* this is a very bad shuffle algorithm but should be good enough
     * for this purpose */
    if (section->nb_tests <= 1)
	return;

    while (nb_swaps--)
    {
	int n = (int) ((double) section->nb_tests * ((double)rand() / (double)RAND_MAX));
	/* swap first element with random element */
	tmp_test = section->tests[0];
	section->tests[0] = section->tests[n];
	section->tests[n] = tmp_test;
    }
}



static void ut_run_suite(section_t *section)
{
    int t;
    if (__ut_shuffle_tests)
	ut_shuffle_suite(section);

    if (! __ut_test_quiet)
        ut_print_section_beginning(section->name);
    for (t = 0; t < section->nb_tests; ++t)
    {
	ut_run_test(section, &section->tests[t]);
    }
    if (! __ut_test_quiet)
        ut_print_section_end();
}


static void ut_list_tests(void)
{
    int s, t;
    for (s = 0; s < __ut_section_count; ++s)
    {
        if (! __ut_test_quiet)
            ut_print_section_beginning_plain(sections[s].name);
	for (t = 0; t < sections[s].nb_tests; ++t)
	{
            color_printf("    %s %3d: %s\n", "TEST",
		      sections[s].tests[t].index + 1, sections[s].tests[t].name);
	}
        if (! __ut_test_quiet)
            ut_print_section_end_plain();
    }
}


static void ut_run_all(void)
{
    int s;
    for (s = 0; s < __ut_section_count; ++s)
    {
	ut_run_suite(&sections[s]);
    }
}


static void ut_run_specific(int testnumber)
{
    int s = 0;
    int t = 0;
    for (s = 0; s < __ut_section_count; ++s)
    {
	if (t + sections[s].nb_tests >= testnumber)
	    break;

	t += sections[s].nb_tests;
    }
    if (s == __ut_section_count)
    {
	fprintf(stderr, "testnumber exceeds total number of tests.\n");
	abort();
    }

    /* relative index in suite */
    t = testnumber - t - 1;

    ut_run_test(&sections[s], &sections[s].tests[t]);
}


int main(int argc, char **argv)
{
    bool success = false;
    int testnumber = -1;
    int i;
    char srcdir[512];

#ifdef WIN32
    /* Disable the crash popup. We don't want a test suite
     * to get stuck waiting for user input! */
    DWORD err_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
    SetErrorMode(err_mode | SEM_NOGPFAULTERRORBOX);
#endif

    __ut_argv0 = argv[0];

    /* random number generator seeding */
    ut_random_init();

    /* hack not to have the "defined but not used" warning */
    if (false)
        ut_end(__UT_RESULT_FAILED);

    /* ut_register_all_tests is generated by ut_build */
    ut_register_all();
    atexit(ut_unregister_all);

    for (i = 1; i < argc; i++)
    {
        if (strlen(argv[i]) != 2 || argv[i][0] != '-')
        {
            fprintf(stderr, "invalid commandline, -h for help\n");
            exit(2);
        }

        switch (argv[i][1])
        {
        case 'd':
            __ut_must_fork = 0;
            break;
        case 'h':
            printf("Usage: %s [OPTION]\n", argv[0]);
            printf("Options:\n");
            printf(
		"  -h : display this help\n");
            printf(
                "  -d : do not fork each test case (ONLY FOR DEBUGGING PURPOSE)\n");
            printf(
                "  -l : list all test cases prefixed by their number\n");
            printf(
                "  -n testnumber : execute only the specific test case,\n"
		"                  which is numbered with testnumber\n");
            printf(
                "  -r : tests of a suite are executed in random order\n");
            printf(
                "  -s : skip lengthy test cases\n");
            printf(
                "  -S : source dir where to search data\n");
            printf(
                "  -v : verbose mode; all debug messages for unit test code are printed\n");
            printf(
                "  -q : quiet mode\n");
            printf(
                "  -x : enable XML output\n");
            exit(0);
            break;
        case 'l':
	    __ut_currently_in = __UT_IN_TEST;
	    ut_list_tests();
	    return 0;
        case 'n':
            if (++i >= argc)
            {
                fprintf(stderr, "missing testnumber\n");
                exit(2);
            }
            errno = 0;
	    testnumber = (int) strtol(argv[i], NULL, 0);
	    if (errno != 0 || testnumber <= 0)
	    {
		fprintf(stderr, "invalid testnumber\n");
		exit(2);
	    }
            break;
        case 'r':
	    __ut_shuffle_tests = true;
	    break;
        case 's':
            __ut_skip_lengthy = true;
            break;
        case 'S':
            if (++i >= argc)
            {
                fprintf(stderr, "missing parameter after -S\n");
                exit(2);
            }
            _ut_snprintf(srcdir, sizeof(srcdir), "srcdir=%s", argv[i]);
            _ut_putenv(srcdir);
            break;
        case 'v':
	    __ut_test_verbose = true;
	    break;
        case 'q':
            __ut_test_quiet = true;
            break;
        case 'x':
            __ut_output_xml = true;
            ut_set_output_xml();
            break;
        default:
            fprintf(stderr, "unknown option '%s', -h for help\n", argv[i]);
            exit(2);
        }
    }

    setbuf(stdout, NULL);

    if (! __ut_test_quiet)
        ut_print_report_beginning(argv[0]);
    if (testnumber == -1)
    {
	ut_run_all();
    }
    else
    {
	ut_run_specific(testnumber);

	/* override __ut_test_count */
	__ut_test_count = 1;
    }
    if (! __ut_test_quiet)
        ut_print_report_end();

    /* XXX Should avoid calculating success twice: currently done here
     * *and* within ut_print_report_end() */
    success = (__ut_pass_count + __ut_skip_count == __ut_test_count);

    return success ? 0 : 1;
}
