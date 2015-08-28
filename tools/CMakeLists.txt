#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_NODES)
    if (WITH_TOOLS)
        if (WIN32)
            install(PROGRAMS exatest_kill.bat DESTINATION ${SBIN_DIR})
        else (WIN32)
            install(PROGRAMS exatest_kill DESTINATION ${SBIN_DIR})
            install(PROGRAMS exa_gdb DESTINATION ${SBIN_DIR})
            install(PROGRAMS exa_enable_valgrind DESTINATION ${SBIN_DIR})
        endif (WIN32 )
    endif (WITH_TOOLS)

    if (WITH_TEST)
        add_subdirectory(test)
    endif (WITH_TEST)
endif (WITH_NODES)
