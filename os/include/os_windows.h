/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_WINDOWS_H
#define OS_WINDOWS_H

#ifndef WIN32
#error "This header can only be used in Windows code"
#endif

#include "os_file.h"
#include "os_syslog.h"
#include "os_error.h"

/* Increase maximum socket in a select (default = 64).
 * Must be done before winsock2.h is included. */
#undef FD_SETSIZE
#define FD_SETSIZE 256

/* winsock2.h *must* be included before windows.h. */
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#ifndef DEBUG
#define os_windows_disable_crash_popup()                  \
    do {                                                  \
        DWORD error_mode;                                 \
        error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX); \
        SetErrorMode(error_mode | SEM_NOGPFAULTERRORBOX); \
    } while (0)
#else
#define os_windows_disable_crash_popup()
#endif

#define os_windows_putenv_from_reg(reg_key, envname, regname) {           \
    /* Careful, remember that the key MUST remain valid during the whole  \
     * life of the process. */                                            \
    static char key[sizeof(envname) + 1 + OS_PATH_MAX];                   \
    char path[OS_PATH_MAX];                                               \
    DWORD len = sizeof(path) - 1;                                         \
    DWORD err = RegGetValue(HKEY_LOCAL_MACHINE, (reg_key),                \
                            (regname), RRF_RT_REG_SZ, NULL, &path, &len); \
                                                                          \
    if (err != ERROR_SUCCESS)                                             \
    {                                                                     \
	os_syslog(OS_SYSLOG_ERROR,                                        \
		  "Cannot get value for registry key '%s': %s (%d).",     \
		  (regname), os_strerror(os_error_from_win(err)), err);   \
	return err;                                                       \
    }                                                                     \
    os_snprintf(key, sizeof(key), "%s=%s", (envname), path);              \
    _putenv(key); }

#endif /* OS_WINDOWS_H */
