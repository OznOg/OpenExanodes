#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_exa_nodeset)
target_link_libraries(ut_exa_nodeset exa_common_user exa_os)

add_unit_test(ut_uuid)
target_link_libraries(ut_uuid exa_common_user exa_os)

add_unit_test(ut_exa_conversion)
target_link_libraries(ut_exa_conversion exa_common_user exa_os)

add_unit_test(ut_exa_env)
target_link_libraries(ut_exa_env exa_common_user exa_os)

add_unit_test(ut_exa_version)
target_link_libraries(ut_exa_version exa_common_user exa_os)

add_unit_test(ut_checksum)
target_link_libraries(ut_checksum exa_common_user)

add_unit_test(ut_exa_nbd_list)
target_link_libraries(ut_exa_nbd_list exa_nbd_list exalogclientfake exa_common_user)
