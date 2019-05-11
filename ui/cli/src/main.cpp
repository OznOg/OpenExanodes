/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/cli.h"
#include "ui/common/include/cli_log.h"

#include "ui/cli/src/command.h"
#include "ui/cli/src/exa_cllicense.h"
#include "ui/cli/src/exa_clcreate.h"
#include "ui/cli/src/exa_cldelete.h"
#include "ui/cli/src/exa_cldiskadd.h"
#include "ui/cli/src/exa_cldiskdel.h"
#include "ui/cli/src/exa_clinfo.h"
#include "ui/cli/src/exa_cllicense.h"
#include "ui/cli/src/exa_clnodeadd.h"
#include "ui/cli/src/exa_clnodedel.h"
#include "ui/cli/src/exa_clnoderecover.h"
#include "ui/cli/src/exa_clnodestart.h"
#include "ui/cli/src/exa_clnodestop.h"
#include "ui/cli/src/exa_clreconnect.h"
#include "ui/cli/src/exa_clstart.h"
#include "ui/cli/src/exa_clstats.h"
#include "ui/cli/src/exa_clstop.h"
#include "ui/cli/src/exa_cltrace.h"
#include "ui/cli/src/exa_cltune.h"
#include "ui/cli/src/exa_dgcreate.h"
#include "ui/cli/src/exa_dgdelete.h"
#include "ui/cli/src/exa_dgdiskadd.h"
#include "ui/cli/src/exa_dgdiskrecover.h"
#include "ui/cli/src/exa_dgstart.h"
#include "ui/cli/src/exa_dgstop.h"
#include "ui/cli/src/exa_vlcreate.h"
#include "ui/cli/src/exa_vldelete.h"
#include "ui/cli/src/exa_vlresize.h"
#include "ui/cli/src/exa_vlstart.h"
#include "ui/cli/src/exa_vlstop.h"
#include "ui/cli/src/exa_vltune.h"
#include "ui/cli/src/exa_expand.h"
#include "ui/cli/src/exa_unexpand.h"
#include "ui/cli/src/exa_makeconfig.h"

#ifdef WITH_FS
#include "ui/cli/src/exa_fscheck.h"
#include "ui/cli/src/exa_fscreate.h"
#include "ui/cli/src/exa_fsdelete.h"
#include "ui/cli/src/exa_fsresize.h"
#include "ui/cli/src/exa_fsstart.h"
#include "ui/cli/src/exa_fsstop.h"
#include "ui/cli/src/exa_fstune.h"
#endif

#ifdef WITH_MONITORING
#include "ui/cli/src/exa_clmonitorstart.h"
#include "ui/cli/src/exa_clmonitorstop.h"
#endif

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

    Cli cli;
    cli.register_cmd_factory("exa_clcreate", command_factory<exa_clcreate> );
    cli.register_cmd_factory("exa_cldelete", command_factory<exa_cldelete> );
    cli.register_cmd_factory("exa_cldiskadd", command_factory<exa_cldiskadd> );
    cli.register_cmd_factory("exa_cldiskdel", command_factory<exa_cldiskdel> );
    cli.register_cmd_factory("exa_clinfo", command_factory<exa_clinfo> );
    cli.register_cmd_factory("exa_cllicense", command_factory<exa_cllicense> );
    cli.register_cmd_factory("exa_clnodeadd", command_factory<exa_clnodeadd> );
    cli.register_cmd_factory("exa_clnodedel", command_factory<exa_clnodedel> );
    cli.register_cmd_factory("exa_clnoderecover", command_factory<exa_clnoderecover> );
    cli.register_cmd_factory("exa_clnodestart", command_factory<exa_clnodestart> );
    cli.register_cmd_factory("exa_clnodestop", command_factory<exa_clnodestop> );
    cli.register_cmd_factory("exa_clreconnect", command_factory<exa_clreconnect> );
    cli.register_cmd_factory("exa_clstart", command_factory<exa_clstart> );
    cli.register_cmd_factory("exa_clstats", command_factory<exa_clstats> );
    cli.register_cmd_factory("exa_clstop", command_factory<exa_clstop> );
    cli.register_cmd_factory("exa_cltrace", command_factory<exa_cltrace> );
    cli.register_cmd_factory("exa_cltune", command_factory<exa_cltune> );
    cli.register_cmd_factory("exa_dgcreate", command_factory<exa_dgcreate> );
    cli.register_cmd_factory("exa_dgdelete", command_factory<exa_dgdelete> );
    cli.register_cmd_factory("exa_dgdiskadd", command_factory<exa_dgdiskadd> );
    cli.register_cmd_factory("exa_dgdiskrecover", command_factory<exa_dgdiskrecover> );
    cli.register_cmd_factory("exa_dgstart", command_factory<exa_dgstart> );
    cli.register_cmd_factory("exa_dgstop", command_factory<exa_dgstop> );
    cli.register_cmd_factory("exa_vlcreate", command_factory<exa_vlcreate> );
    cli.register_cmd_factory("exa_vldelete", command_factory<exa_vldelete> );
    cli.register_cmd_factory("exa_vlresize", command_factory<exa_vlresize> );
    cli.register_cmd_factory("exa_vlstart", command_factory<exa_vlstart> );
    cli.register_cmd_factory("exa_vlstop", command_factory<exa_vlstop> );
    cli.register_cmd_factory("exa_vltune", command_factory<exa_vltune> );
    cli.register_cmd_factory("exa_unexpand", command_factory<exa_unexpand> );
    cli.register_cmd_factory("exa_expand", command_factory<exa_expand> );
    cli.register_cmd_factory("exa_makeconfig", command_factory<exa_makeconfig> );

#ifdef WITH_FS
    cli.register_cmd_factory("exa_fscheck", command_factory<exa_fscheck> );
    cli.register_cmd_factory("exa_fscreate", command_factory<exa_fscreate> );
    cli.register_cmd_factory("exa_fsdelete", command_factory<exa_fsdelete> );
    cli.register_cmd_factory("exa_fsresize", command_factory<exa_fsresize> );
    cli.register_cmd_factory("exa_fsstart", command_factory<exa_fsstart> );
    cli.register_cmd_factory("exa_fsstop", command_factory<exa_fsstop> );
    cli.register_cmd_factory("exa_fstune", command_factory<exa_fstune> );
#endif

#ifdef WITH_MONITORING
    cli.register_cmd_factory("exa_clmonitorstart", command_factory<exa_clmonitorstart> );
    cli.register_cmd_factory("exa_clmonitorstop", command_factory<exa_clmonitorstop> );
#endif
    try
    {
        char *arg = argv[0];

        std::string scmd((os_program_name(arg)));

        boost::to_lower(scmd);

        Command::factory_t factory = cli.find_cmd_factory(scmd);

        /*
         * If we couldn't find the command in the name we were invoked, try
         * again with the first parameter.
         */
        if ((factory == NULL) && argc > 1)
        {
            ++argv;
            --argc;

            factory = cli.find_cmd_factory(argv[0]);
        }

        if (factory == NULL)
        {
            if (scmd != "exa_cli")
                exa_cli_error("Unknown command %s\n", scmd.c_str());
            cli.usage();
            throw CommandException(EXA_ERR_CMD_PARSING);
        }

        std::shared_ptr<Command> cmd = (*factory)(argc, argv);
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

