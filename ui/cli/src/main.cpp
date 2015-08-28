/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/cli.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_network.h"
#include "os/include/os_random.h"
#include "os/include/os_file.h"

#include <stdlib.h>

#include <boost/algorithm/string.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    int return_value(0);
    exa_cli_log_level_t log_level;

    if (exa_verb_from_str(&log_level, getenv("EXA_CLI_LOGLEVEL")))
        set_exa_verb(log_level);

    os_net_init();

    /*
     * Init the seed to have a good random
     */
    os_random_init();

    try
    {
        char *arg = argv[0];

        std::string scmd((os_program_name(arg)));

        boost::to_lower(scmd);

        Command::factory_t factory = Cli::instance().find_cmd_factory(scmd);

        /*
         * If we couldn't find the command in the name we were invoked, try
         * again with the first parameter.
         */
        if ((factory == NULL) && argc > 1)
        {
            ++argv;
            --argc;

            factory = Cli::instance().find_cmd_factory(argv[0]);
        }

        if (factory == NULL)
        {
            if (scmd != "exa_cli")
                exa_cli_error("Unknown command %s\n", scmd.c_str());
            Cli::instance().usage();
            throw CommandException(EXA_ERR_CMD_PARSING);
        }

        boost::shared_ptr<Command> cmd = (*factory)(argc, argv);
        cmd->run();
    }
    catch (exa::Exception &e)
    {
        exa_error_code error_code(e.error_code());

        /* Display string if any */
        if (strcmp(e.what(), "") != 0)
            exa_cli_error("\n%sERROR%s: %s\n", COLOR_ERROR, COLOR_NORM, e.what());

        /* cf old EXA_CLI_EXIT macro */
        if (get_error_type(error_code) != ERR_TYPE_ERROR)
            return_value = EXA_SUCCESS;
        else if (error_code > EXA_ERR_DEFAULT &&
                 error_code < EXA_ERR_DUMMY_END_OF_CLI_SECTION)
            return_value = error_code;
        else
            return_value = EXA_ERR_DEFAULT;
    }
    catch (std::exception &e)
    {
        exa_cli_error("\n%sERROR%s: %s\n", COLOR_ERROR, COLOR_NORM, e.what());
        return_value = EXA_ERR_DEFAULT;
    }
    catch (...)
    {
        exa_cli_error("\n%sERROR%s: Unknown error\n", COLOR_ERROR, COLOR_NORM);
        return_value = EXA_ERR_DEFAULT;
    }

    os_random_cleanup();
    os_net_cleanup();

    return return_value;
}


