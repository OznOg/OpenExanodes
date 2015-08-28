#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_rdev_config
    ../src/rdev_config.c)

target_link_libraries(ut_rdev_config
        exa_common_user
	exa_license
        exa_os)

add_unit_test(ut_broken_disk_table
    ../src/broken_disk_table.c)

target_link_libraries(ut_broken_disk_table
    exa_common_user
    exa_os)
