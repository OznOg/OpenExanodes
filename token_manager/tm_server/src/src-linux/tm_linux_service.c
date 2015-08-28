/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdio.h>
#include <stdlib.h>
#include "token_manager/tm_server/src/token_manager.h"
#include "common/include/exa_conversion.h"
#include "os/include/os_error.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define DEFAULT_TOKEN_FILE  "/tmp/" TOKEN_MANAGER_DEFAULT_TOKEN_FILE

static void sighandler(int sig)
{
    switch (sig)
    {
        case SIGTERM:
            token_manager_thread_stop();
            break;
        case SIGHUP:
            token_manager_reopen_log();
            break;
        default:
            break;
    }
}

static int install_sighandler(void)
{
    struct sigaction action;
    sigset_t sigmask;
    int r;

    signal(SIGPIPE, SIG_IGN);

    r = sigemptyset(&sigmask);
    if (r != 0)
        return r;

    r = sigaddset(&sigmask, SIGTERM);
    if (r != 0)
        return r;
    r = sigaddset(&sigmask, SIGHUP);
    if (r != 0)
        return r;

    action.sa_handler = sighandler;
    action.sa_mask    = sigmask;
    action.sa_flags   = 0;

    r = sigaction(SIGTERM, &action, 0);
    if (r != 0)
        return r;
    r = sigaction(SIGHUP, &action, 0);
    if (r != 0)
        return r;

    r = sigprocmask(SIG_UNBLOCK, &sigmask, 0);

    return r;
}

int main(int argc, char *argv[])
{
    token_manager_data_t data;
    const char *port_env_var;
    const char *priv_port_env_var;
    int fd;
    pid_t pid;

    data.file = getenv(TOKEN_MANAGER_FILE_ENV_VAR);
    if (data.file == NULL || *data.file == '\0')
        data.file = DEFAULT_TOKEN_FILE;

    data.logfile = getenv(TOKEN_MANAGER_LOGFILE_ENV_VAR);

    port_env_var = getenv(TOKEN_MANAGER_PORT_ENV_VAR);
    if (port_env_var == NULL)
        data.port = 0; /* use default port */
    else if (to_uint16(port_env_var, &data.port) != 0)
    {
        fprintf(stderr, "Invalid port: '%s'\n", port_env_var);
        exit(1);
    }

    priv_port_env_var = getenv(TOKEN_MANAGER_PRIVPORT_ENV_VAR);
    if (priv_port_env_var == NULL)
        data.priv_port = 0; /* use default port */
    else if (to_uint16(priv_port_env_var, &data.priv_port) != 0)
    {
        fprintf(stderr, "Invalid privileged port: '%s'\n", priv_port_env_var);
        exit(1);
    }

    data.debug = getenv(TOKEN_MANAGER_DEBUG) != NULL;

    if (data.logfile != NULL)
    {
        pid = fork();

        if (pid < 0)
            exit(errno);
        if (pid > 0)
            exit(0);    /* Parent exits */

        setsid();       /* Detach */

        /* get rid of stdin, stdout, stderr. */
        for (fd = getdtablesize(); fd >= 0 ; fd--)
            close(fd);
        fd = open("/dev/null", O_RDWR);
        if (dup(fd) < 0)
            return 1;
        if (dup(fd) < 0)
            return 1;
    }

    if (chdir("/tmp") < 0)
        return 1;

    if (install_sighandler() != 0)
        return 1;

    token_manager_thread(&data);

    return data.result;
}
