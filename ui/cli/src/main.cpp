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

#ifdef WITH_TOOLS
#include "ui/cli/src/exa_dgreset.h"
#include "ui/cli/src/exa_dgcheck.h"
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

    Cli<
#ifdef WITH_COMMANDS
        exa_clcreate,
        exa_cldelete,
        exa_cldiskadd,
        exa_cldiskdel,
        exa_clinfo,
        exa_cllicense,
        exa_clnodeadd,
        exa_clnodedel,
        exa_clnoderecover,
        exa_clnodestart,
        exa_clnodestop,
        exa_clreconnect,
        exa_clstart,
        exa_clstats,
        exa_clstop,
        exa_cltrace,
        exa_cltune,
        exa_dgcreate,
        exa_dgstart,
        exa_dgstop,
        exa_dgdelete,
        exa_dgdiskadd,
        exa_dgdiskrecover,
        exa_vlcreate,
        exa_vldelete,
        exa_vlresize,
        exa_vlstart,
        exa_vlstop,
        exa_vltune,

#ifdef WITH_FS
        exa_fscheck,
        exa_fscreate,
        exa_fsdelete,
        exa_fsresize,
        exa_fsstart,
        exa_fsstop,
        exa_fstune,
#endif

#ifdef WITH_MONITORING
        exa_clmonitorstart,
        exa_clmonitorstop,
#endif

        exa_unexpand,
        exa_expand,
        exa_makeconfig
#endif // WITH_COMMANDS

#ifdef WITH_TOOLS
#ifdef WITH_COMMANDS
        , // need a comma separator for template arguments...
          // ths is quite ugly, but I did not find any better solution for now
#endif
        exa_dgreset,
        exa_dgcheck
#endif

        > cli;
    try
    {
        char *arg = argv[0];

        std::string scmd((os_program_name(arg)));

        boost::to_lower(scmd);

        auto cmd = cli.find_cmd(scmd);

        /*
         * If we couldn't find the command in the name we were invoked, try
         * again with the first parameter.
         */
        if ((cmd == nullptr) && argc > 1)
        {
            ++argv;
            --argc;

            cmd = cli.find_cmd(argv[0]);
        }

        if (cmd == nullptr)
        {
            if (scmd != "exa_cli")
                exa_cli_error("Unknown command %s\n", scmd.c_str());
            cli.usage();
            throw CommandException(EXA_ERR_CMD_PARSING);
        }

        cmd->run(argc, argv);
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

