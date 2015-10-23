/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/* FIXME We must have the Windows side tested too at some point! */

#ifndef WIN32
#include <stdlib.h>
#include <unistd.h>
#endif

#include <unit_testing.h>

#include "os/include/os_inttypes.h"
#include "os/include/os_random.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"
#include "os/include/os_syslog.h"
#include "os/include/os_stdio.h"

/* Identifier to log with */
#define UNIT_TEST_IDENT    "Unit test"
#define UNIT_TEST_IDENT_2  "Unit test (2)"

#define UID_STR_LEN  16
#define UID_STR_FMT "%016" PRIx64

/* Generate a unique string id to be used in our messages
   so that each message is unique and we can check its
   occurrence in the system log */
static const char *__gen_uid_str(void)
{
    static char uid_str[UID_STR_LEN + 1];
    uint64_t uid;

    os_get_random_bytes(&uid, sizeof(uid));
    os_snprintf(uid_str, sizeof(uid_str), UID_STR_FMT, uid);

    return uid_str;
}

static void __check_logged(const char *uid_str, const char *ident)
{
#ifndef WIN32
    static char grep_fmt[] = "journalctl | grep -q '%s: %s'";
    static char grep_cmd[256];
    int ret, retry = 50;

    os_snprintf(grep_cmd, sizeof(grep_cmd), grep_fmt, ident, uid_str);

    /* syslog() is asynchronous so we must give syslogd some time to
     * write the event */

    do {
        os_millisleep(20);

        ret = system(grep_cmd);
    } while (ret != 0 && --retry != 0);

    UT_ASSERT(WIFEXITED(ret));
    UT_ASSERT_EQUAL(0, WEXITSTATUS(ret));
#endif
}

ut_setup()
{
    /* XXX The initialization of os_random is performed for each test case,
     * thus we can't really guarantee that all test cases will have truly
     * unique uids. It should be done only once for all test cases, but
     * this is not feasible. */
    os_random_init();
}

ut_cleanup()
{
    os_random_cleanup();
}

ut_test(log_after_open_is_ok)
{
    const char *uid_str = __gen_uid_str();

    os_openlog(UNIT_TEST_IDENT);
    os_syslog(OS_SYSLOG_INFO, "%s", uid_str);
    os_closelog();

    __check_logged(uid_str, UNIT_TEST_IDENT);
}

ut_test(log_without_open_logs_with_ident_unknown)
{
    const char *uid_str = __gen_uid_str();

    os_syslog(OS_SYSLOG_INFO, "%s", uid_str);

    __check_logged(uid_str, "");
}

ut_test(open_then_reopen_with_diff_idents)
{
    char uid_str1[UID_STR_LEN + 1];
    char uid_str2[UID_STR_LEN + 1];

    os_openlog(UNIT_TEST_IDENT);
    strlcpy(uid_str1, __gen_uid_str(), sizeof(uid_str1));
    os_syslog(OS_SYSLOG_INFO, "%s", uid_str1);

    os_openlog(UNIT_TEST_IDENT_2);
    strlcpy(uid_str2, __gen_uid_str(), sizeof(uid_str2));
    os_syslog(OS_SYSLOG_INFO, "%s", uid_str2);

    __check_logged(uid_str1, UNIT_TEST_IDENT);

    __check_logged(uid_str2, UNIT_TEST_IDENT_2);
}
