#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(supcluster STATIC
    sup_cluster.c
    sup_view.c)

add_library(sup_examsg STATIC sup_examsg.c)

add_library(supclique STATIC sup_clique.c)

add_library(supdebug STATIC sup_debug.c)

add_library(supsimdebug STATIC sup_debug.c)
set_target_properties(supsimdebug PROPERTIES COMPILE_DEFINITIONS WITH_SUPSIM)

if (WITH_SUPSIM)
    add_library(sup_sim STATIC sup_sim.c)
    set(COMMUNICATION_LAYER sup_sim)
else (WITH_SUPSIM)
    set(DAEMON_CHILD os_daemon_child)
    set(COMMUNICATION_LAYER sup_examsg examsg)
endif (WITH_SUPSIM)


add_executable(exa_csupd exa_csupd.c)
target_link_libraries(exa_csupd
    ${DAEMON_CHILD}
    supcluster
    supclique
    supdebug
    ${COMMUNICATION_LAYER}
    exalogclient
    exa_common_user
    exa_os)

install(TARGETS exa_csupd DESTINATION ${SBIN_DIR})
