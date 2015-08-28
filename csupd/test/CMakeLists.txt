#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_library(helpers STATIC sup_helpers.c)

add_unit_test(ut_sup_clique)

set(LIBS
    supcluster
    supsimdebug
    exa_common_user
    exalogclientfake
    exa_os)

target_link_libraries(ut_sup_clique helpers supclique ${LIBS})

add_unit_test(ut_sup_view)

target_link_libraries(ut_sup_view ${LIBS})

add_unit_test(ut_sup_cluster)

target_link_libraries(ut_sup_cluster ${LIBS})

add_unit_test(ut_sup_helpers)

target_link_libraries(ut_sup_helpers helpers ${LIBS})


