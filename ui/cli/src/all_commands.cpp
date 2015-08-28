/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

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

Command::factory_t exa_clcreate_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clcreate", command_factory<exa_clcreate> );

Command::factory_t exa_cldelete_factory =
    Cli::instance().register_cmd_factory
                  ("exa_cldelete", command_factory<exa_cldelete> );

Command::factory_t exa_cldiskadd_factory =
    Cli::instance().register_cmd_factory
                  ("exa_cldiskadd", command_factory<exa_cldiskadd> );

Command::factory_t exa_cldiskdel_factory =
    Cli::instance().register_cmd_factory
                  ("exa_cldiskdel", command_factory<exa_cldiskdel> );

Command::factory_t exa_clinfo_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clinfo", command_factory<exa_clinfo> );

Command::factory_t exa_cllicense_factory =
    Cli::instance().register_cmd_factory
                  ("exa_cllicense", command_factory<exa_cllicense> );

Command::factory_t exa_clnodeadd_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clnodeadd", command_factory<exa_clnodeadd> );

Command::factory_t exa_clnodedel_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clnodedel", command_factory<exa_clnodedel> );

Command::factory_t exa_clnoderecover_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clnoderecover", command_factory<exa_clnoderecover> );

Command::factory_t exa_clnodestart_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clnodestart", command_factory<exa_clnodestart> );

Command::factory_t exa_clnodestop_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clnodestop", command_factory<exa_clnodestop> );

Command::factory_t exa_clreconnect_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clreconnect", command_factory<exa_clreconnect> );

Command::factory_t exa_clstart_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clstart", command_factory<exa_clstart> );

Command::factory_t exa_clstats_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clstats", command_factory<exa_clstats> );

Command::factory_t exa_clstop_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clstop", command_factory<exa_clstop> );

Command::factory_t exa_cltrace_factory =
    Cli::instance().register_cmd_factory
                  ("exa_cltrace", command_factory<exa_cltrace> );

Command::factory_t exa_cltune_factory =
    Cli::instance().register_cmd_factory
                  ("exa_cltune", command_factory<exa_cltune> );

Command::factory_t exa_dgcreate_factory =
    Cli::instance().register_cmd_factory
                  ("exa_dgcreate", command_factory<exa_dgcreate> );

Command::factory_t exa_dgdelete_factory =
    Cli::instance().register_cmd_factory
                  ("exa_dgdelete", command_factory<exa_dgdelete> );

Command::factory_t exa_dgdiskadd_factory =
    Cli::instance().register_cmd_factory
                  ("exa_dgdiskadd", command_factory<exa_dgdiskadd> );

Command::factory_t exa_dgdiskrecover_factory =
    Cli::instance().register_cmd_factory
                  ("exa_dgdiskrecover", command_factory<exa_dgdiskrecover> );

Command::factory_t exa_dgstart_factory =
    Cli::instance().register_cmd_factory
                  ("exa_dgstart", command_factory<exa_dgstart> );

Command::factory_t exa_dgstop_factory =
    Cli::instance().register_cmd_factory
                  ("exa_dgstop", command_factory<exa_dgstop> );

Command::factory_t exa_vlcreate_factory =
    Cli::instance().register_cmd_factory
                  ("exa_vlcreate", command_factory<exa_vlcreate> );

Command::factory_t exa_vldelete_factory =
    Cli::instance().register_cmd_factory
                  ("exa_vldelete", command_factory<exa_vldelete> );

Command::factory_t exa_vlresize_factory =
    Cli::instance().register_cmd_factory
                  ("exa_vlresize", command_factory<exa_vlresize> );

Command::factory_t exa_vlstart_factory =
    Cli::instance().register_cmd_factory
                  ("exa_vlstart", command_factory<exa_vlstart> );

Command::factory_t exa_vlstop_factory =
    Cli::instance().register_cmd_factory
                  ("exa_vlstop", command_factory<exa_vlstop> );

Command::factory_t exa_vltune_factory =
    Cli::instance().register_cmd_factory
                  ("exa_vltune", command_factory<exa_vltune> );

Command::factory_t exa_unexpand_factory =
    Cli::instance().register_cmd_factory
                  ("exa_unexpand", command_factory<exa_unexpand> );

Command::factory_t exa_expand_factory =
    Cli::instance().register_cmd_factory
                  ("exa_expand", command_factory<exa_expand> );

Command::factory_t exa_makeconfig_factory =
    Cli::instance().register_cmd_factory
                  ("exa_makeconfig", command_factory<exa_makeconfig> );

#ifdef WITH_FS
Command::factory_t exa_fscheck_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fscheck", command_factory<exa_fscheck> );

Command::factory_t exa_fscreate_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fscreate", command_factory<exa_fscreate> );

Command::factory_t exa_fsdelete_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fsdelete", command_factory<exa_fsdelete> );

Command::factory_t exa_fsresize_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fsresize", command_factory<exa_fsresize> );

Command::factory_t exa_fsstart_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fsstart", command_factory<exa_fsstart> );

Command::factory_t exa_fsstop_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fsstop", command_factory<exa_fsstop> );

Command::factory_t exa_fstune_factory =
    Cli::instance().register_cmd_factory
                  ("exa_fstune", command_factory<exa_fstune> );
#endif

#ifdef WITH_MONITORING
Command::factory_t exa_clmonitorstart_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clmonitorstart", command_factory<exa_clmonitorstart> );

Command::factory_t exa_clmonitorstop_factory =
    Cli::instance().register_cmd_factory
                  ("exa_clmonitorstop", command_factory<exa_clmonitorstop> );
#endif
