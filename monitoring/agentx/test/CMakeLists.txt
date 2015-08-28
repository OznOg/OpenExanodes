#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_md_heartbeat ../src/md_heartbeat.c)

target_link_libraries(ut_md_heartbeat
    md_com
    exa_os
    ${LIBPTHREAD})
