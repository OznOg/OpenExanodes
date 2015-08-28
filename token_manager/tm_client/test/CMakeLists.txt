#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

# XXX Using so many source files from token_manager means we should have a
#     token_manager library.

add_unit_test(ut_tm_client
              ../../tm_server/src/token_manager.c
              ../../tm_server/src/tm_tokens.c
              ../../tm_server/src/tm_file.c
              ../../tm_server/src/tm_err.c
              ../src/tm_client.c)

set(LIBS
    exa_common_user
    exa_os)

target_link_libraries(ut_tm_client ${LIBS})
