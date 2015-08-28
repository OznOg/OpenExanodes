#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

# FIXME Most targets have a bunch of common dependencies. Should make
# a library of these dependencies to avoid recompiling them as many
# times as there are commands!

add_unit_test(ut_exa_vlstart
    ../src/exa_vlstart.cpp)
target_link_libraries(ut_exa_vlstart
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os)

add_unit_test(ut_exa_vlstop
    ../src/exa_vlstop.cpp)
target_link_libraries(ut_exa_vlstop
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os)

add_unit_test(ut_exa_vlcreate
    ../src/exa_vlcreate.cpp)
target_link_libraries(ut_exa_vlcreate
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os)

add_unit_test(ut_exa_clinfo
    exa_clinfo_ut_helper.cpp
    ../src/exa_clinfo.cpp)
target_link_libraries(ut_exa_clinfo
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os)

add_unit_test(ut_exa_vltune
    ../src/exa_vltune.cpp)
target_link_libraries(ut_exa_vltune
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os)

add_unit_test(ut_exa_clcreate
    ../src/exa_clcreate.cpp)
target_link_libraries(ut_exa_clcreate
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os)

