#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

# If not root, this test may fail to unlink sysv objects.

if (WITH_UT_ROOT)
    add_unit_test(ut_logd ../src/logd.c)
    target_link_libraries(ut_logd exa_common_user
        exa_os exalogclientfake
	${LIBWS2_32})
    set_tests_properties(ut_logd PROPERTIES ENVIRONMENT "EXANODES_LOG_DIR=.")
endif (WITH_UT_ROOT)

