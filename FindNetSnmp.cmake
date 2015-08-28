#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#


if (NETSNMP_INCLUDE_DIR)
# Already in cache, be silent
    set(NetSnmp_FIND_QUIETLY TRUE)
endif (NETSNMP_INCLUDE_DIR)

find_path(NETSNMP_INCLUDE_DIR
    NAMES net-snmp/net-snmp-config.h)

find_program(NETSNMP_CONFIG NAMES net-snmp-config)

if (NETSNMP_CONFIG)

    execute_process(
        COMMAND ${NETSNMP_CONFIG} --agent-libs
        OUTPUT_VARIABLE NETSNMP_CONFIG_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NETSNMP_CONFIG_RESULT)

    if ("${NETSNMP_CONFIG_RESULT}" MATCHES "^0$")

        # Convert the compile FLAGs to a CMake list.
        string(REGEX REPLACE " +" ";"
            NETSNMP_CONFIG_LIBS "${NETSNMP_CONFIG_LIBS}")

        # Look for -L flags for directories and -l flags for library names.
        set(NETSNMP_LIBRARY_DIRS)
        set(NETSNMP_LIBRARIES)
        set(NETSNMP_LINK_FLAGS)
        set(NETSNMP_RPATH)
        foreach (FLAG ${NETSNMP_CONFIG_LIBS})
            if ("${FLAG}" MATCHES "^-L")
                string(REGEX REPLACE "^-L" "" DIR "${FLAG}")
                file(TO_CMAKE_PATH "${DIR}" DIR)
                set(NETSNMP_LIBRARY_DIRS ${NETSNMP_LIBRARY_DIRS} "${DIR}")
            elseif ("${FLAG}" MATCHES "^-l")
                string(REGEX REPLACE "^-l" "" NAME "${FLAG}")
                set(NETSNMP_LIBRARIES ${NETSNMP_LIBRARIES} "${NAME}")
            elseif ("${FLAG}" MATCHES "^-Wl,-rpath,")
                string(REGEX REPLACE "^-Wl,-rpath," "" DIR "${FLAG}")
                set(NETSNMP_RPATH ${NETSNMP_RPATH} "${DIR}")
            elseif ("${FLAG}" MATCHES "^-Wl")
                set(NETSNMP_LINK_FLAGS "${NETSNMP_LINK_FLAGS} ${FLAG}")
            elseif ("${FLAG}" MATCHES "\\.a$")
                set(NETSNMP_LIBRARIES ${NETSNMP_LIBRARIES} "${FLAG}")
            endif ("${FLAG}" MATCHES "^-L")
        endforeach (FLAG)

    endif ("${NETSNMP_CONFIG_RESULT}" MATCHES "^0$")

endif (NETSNMP_CONFIG)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NetSnmp
    DEFAULT_MSG
    NETSNMP_CONFIG
    NETSNMP_INCLUDE_DIR
    NETSNMP_LIBRARY_DIRS
    NETSNMP_LIBRARIES
    NETSNMP_LINK_FLAGS)

mark_as_advanced(
    NETSNMP_CONFIG
    NETSNMP_INCLUDE_DIR
    NETSNMP_LIBRARIY_DIRS
    NETSNMP_LIBRARIES
    NETSNMP_LINK_FLAGS
    NETSNMP_RPATH)
