#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

# ut_objpool

add_unit_test(ut_objpool
    ../src/objpoolapi.c)

target_link_libraries(ut_objpool
    exa_common_user
    exa_os
    exalogclientfake)

# ut_mailbox. If not root, this test may fail to unlink sysv objects.

if (WITH_UT_ROOT)

add_unit_test(ut_mailbox
    ../src/objpoolapi.c
    ../src/ringbuf.c
    ../src/mailbox.c)

target_link_libraries(ut_mailbox
    exa_common_user
    exa_os
    exalogclientfake)

endif (WITH_UT_ROOT)

# ut_ringbuf

add_unit_test(ut_ringbuf
    ../src/ringbuf.c)

target_link_libraries(ut_ringbuf
    exa_common_user
    exa_os
    exalogclientfake)

# ut_examsgapi. This test fails when /dev/shm is not mounted.

if (WITH_UT_ROOT)

add_unit_test(ut_examsgapi
    ../src/examsgapi.c
    ../src/objpoolapi.c
    ../src/mailbox.c
    ../src/ringbuf.c)

target_link_libraries(ut_examsgapi
    exa_common_user
    exa_os
    exalogclientfake)

endif (WITH_UT_ROOT)

