/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "token_manager/tm_client/include/tm_client.h"
#include "token_manager/tm_server/include/token_msg.h"
#include "token_manager/tm_server/include/tm_file.h"
#include "token_manager/tm_server/include/tm_server.h"

#include "common/include/exa_conversion.h"

#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_getopt.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_network.h"
#include "os/include/os_string.h"

#include <stdlib.h>
#include <stdio.h>

static const char *self;

static void usage(void)
{
    fprintf(stderr,
        "Token Manager tool.\n"
        "\n"
        "Usage: %s [OPTIONS] --force-release <token>\n"
        "       %s --dump <file>\n"
        "\n"
        "Where <token> is an UUID of the form xxxxxxxx:xxxxxxxx:xxxxxxxx:xxxxxxxx\n"
        "              (the UUID of a cluster's token is the cluster's UUID).\n"
        "\n"
        "      <file> is a token file to be dumped on standard output.\n"
        "             Note that if the file doesn't exist, the dump will report\n"
        "             0 token held."
        "\n"
        "OPTIONS:\n"
        "   --help          This help\n"
        "   --port <port>   The token manager's privileged port (%"PRIu16" by default)\n"
        "\n"
        "WARNING!\n"
        "    When --force-release is specified, the token is released regardless\n"
        "    of whether it is held by a node.\n"
        "    Do NOT make the Token Manager forcefully release a token unless you know\n"
        "    for sure the node holding the token is dead *and* won't ever come back.\n"
        "\n", self, self, TOKEN_MANAGER_DEFAULT_PRIV_PORT);
}

static int release_token(uint16_t priv_port, const exa_uuid_t *uuid)
{
    int err;
    token_manager_t *tm;

    /* Connection */
    if (priv_port == 0)
        priv_port = TOKEN_MANAGER_DEFAULT_PRIV_PORT;

    err = os_net_init();
    if (err != 0)
    {
        fprintf(stderr, "Failed initializing networking: %s (%d)\n",
                os_strerror(err), err);
        goto done;
    }

    err = tm_init(&tm, "127.0.0.1", priv_port);
    if (err != 0)
    {
        fprintf(stderr, "Failed initializing Token Manager: %s (%d)\n",
                os_strerror(-err), err);
        goto done;
    }

    err = tm_connect(tm);
    if (err != 0)
    {
        fprintf(stderr, "Failed connecting to Token Manager: %s (%d)\n",
                os_strerror(-err), err);
        goto done;
    }

    err = tm_force_token_release(tm, uuid);
    tm_disconnect(tm);
    tm_free(&tm);

    if (err != 0)
    {
        fprintf(stderr, "Failed releasing token "UUID_FMT": %s (%d)\n",
                UUID_VAL(uuid), os_strerror(-err), err);
        goto done;
    }

    printf("Token "UUID_FMT" has been released.\n", UUID_VAL(uuid));

done:
    os_net_cleanup();

    return err;
}

static int dump_tokens(const char *token_file)
{
    int i, err = 0;
    uint64_t num_tok = TM_TOKENS_MAX;
    token_t toks[TM_TOKENS_MAX];

    err = tm_file_load(token_file, toks, &num_tok);
    if (err < 0)
    {
        fprintf(stderr, "Invalid token file: '%s'", token_file);
        return err;
    }

    printf("%"PRIu64" %s currently held.\n", num_tok, num_tok > 1 ? "tokens" : "token");
    for (i = 0; i < num_tok; i++)
    {
        const token_t *t = &toks[i];
        printf("    "UUID_FMT" held by node %"PRInodeid" (%s)\n", UUID_VAL(&t->uuid), t->holder, t->holder_addr);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    static struct option long_opts[] =
        {
            { "dump",           required_argument, NULL, 'd' },
            { "force-release",  required_argument, NULL, 'f' },
            { "help",           no_argument,       NULL, 'h' },
            { "port",           required_argument, NULL, 'p' },
            { NULL,             0                , NULL, 0   }
        };
    int long_idx, err;
    exa_uuid_t uuid;
    uint16_t priv_port = 0;
    char token_file[OS_PATH_MAX];
    bool do_release = false, do_dump = false, port_set = false;

    /* Note: this modifies argv[0] */
    self = os_program_name(argv[0]);

    uuid_zero(&uuid);

    while (true)
    {
        int c = os_getopt_long(argc, argv, "d:f:hp:", long_opts, &long_idx);
        if (c == -1)
            break;

        switch (c)
        {
        case 'd':
            os_strlcpy(token_file, optarg, sizeof(token_file));
            do_dump = true;
            break;

        case 'h':
            usage();
            exit(0);
            break;

        case 'p':
            if (to_uint16(optarg, &priv_port) != 0 || priv_port == 0)
            {
                fprintf(stderr, "Invalid port: '%s'\n", optarg);
                exit(1);
            }
            port_set = true;
            break;

        case 'f':
            if (uuid_scan(optarg, &uuid) != 0 || uuid_is_zero(&uuid))
            {
                fprintf(stderr, "Invalid token: '%s'\n", optarg);
                exit(1);
            }
            do_release = true;
            break;

        default:
            exit(1);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Invalid command line\n");
        exit(1);
    }

    if (!do_release && !do_dump)
    {
        fprintf(stderr, "No valid command specified\n");
        exit(1);
    }

    if (do_release && do_dump)
    {
        fprintf(stderr,
                "--force-release and --dump cannot be specifed at the same time\n");
        exit(1);
    }

    if (do_dump && port_set)
    {
        fprintf(stderr, "--dump and --port cannot be specified at the same time\n");
        exit(1);
    }

    if (do_release)
        err = release_token(priv_port, &uuid);
    else if (do_dump)
        err = dump_tokens(token_file);
    else
        err = 0;

    return err == 0 ? 0 : 1;
}
