#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(examsgd_client examsgd_client.c)

add_executable(exa_msgd
    examsgd.c
    network.c
    iface.c)

target_link_libraries(exa_msgd
    os_daemon_child
    exalogclient
    examsg
    exa_common_user
    exa_os
    ${LIBYAOURT})

install(TARGETS exa_msgd DESTINATION ${SBIN_DIR})
