#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_executable(exa_md
    md_controller.c
    md_trap_sender.c
    md_event_list.c
    md_srv_com.c
    exa_md.c)

target_link_libraries(exa_md
    exalogclient
    md_com
    md_com_socket
    md_common
    examsg
    exa_common_user
    exa_os
    os_daemon_child
    ${LIBPTHREAD}
    ${LIBRT})

install(TARGETS exa_md DESTINATION ${SBIN_DIR})
