#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

set(LIBS
    exa_common_user
    exa_os)

add_unit_test(ut_tm_tokens
              ../src/tm_tokens.c
              ../src/tm_file.c
              ../src/tm_err.c)

target_link_libraries(ut_tm_tokens ${LIBS})

add_unit_test(ut_tm_file
              ../src/tm_file.c)

target_link_libraries(ut_tm_file ${LIBS})
