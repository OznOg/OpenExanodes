#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_lun)
target_link_libraries(ut_lun lun exa_common_user exa_os)

add_unit_test(ut_iqn)
target_link_libraries(ut_iqn iqn exa_common_user exa_os)

add_unit_test(ut_iqn_filter)
target_link_libraries(ut_iqn_filter iqn_filter exa_common_user exa_os)

add_unit_test(ut_parameters
    ../src/parameters.c)

target_link_libraries(ut_parameters exalogclientfake exa_common_user exa_os)

add_unit_test(ut_iscsi_negociation
    ../src/parameters.c
    ../src/iscsi_negociation.c)

target_link_libraries(ut_iscsi_negociation exalogclientfake exa_common_user exa_os)
