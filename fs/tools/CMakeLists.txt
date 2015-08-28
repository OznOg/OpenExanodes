#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_executable(exa_fstool
    exa_fstool.c)

target_link_libraries(exa_fstool
    exafsd_client
    examsg
    exa_common_user
    daemon_lightclient
    exalogclientfake
    exa_os)

install(TARGETS exa_fstool DESTINATION ${SBIN_DIR})
