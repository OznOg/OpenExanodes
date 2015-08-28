#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

set(lun_lib lun)
set(iqn_lib iqn)

add_unit_test(ut_export
    ../src/export.c)

target_link_libraries(ut_export
    exa_config
    exa_common_user
    exa_export
    exalogclientfake
    exa_os
    iqn
    iqn_filter)
