#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

function(__add_unit_test NAME)
add_unit_test(${NAME} ${ARGN})
target_link_libraries(${NAME}
    ${XML2_LIBS}
    exa_config
    exa_common_user
    exa_os)
endfunction(__add_unit_test NAME)


__add_unit_test(ut_clustercache
    ../src/clustercache.cpp)

__add_unit_test(ut_admindcommand
    ../src/admindcommand.cpp)

__add_unit_test(ut_exa_expand
    ../src/exa_expand.cpp)

__add_unit_test(ut_config_check
    ../src/config_check.cpp
    ../src/cli_log.cpp
    ../../cli/src/cli_log_impl.cpp)

__add_unit_test(ut_split_node_disk
    ../src/split_node_disk.cpp)

# Hack to get admind/src/xml_proto/xml_protocol_version.h generated.
# Theorically adding it to the sources of ut_admindcommand should do the job
# (see cmake manual) but it seems it does not work across directories.
add_dependencies(ut_admindcommand xml_proto)

