#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(exa_ui_common STATIC
    admindclient.cpp
    admindclient_request.cpp
    admindclient_seeker.cpp
    admindcommand.cpp
    admindmessage.cpp
    cli_log.cpp
    clustercache.cpp
    common_utils.cpp
    config_check.cpp
    exa_conversion.cpp
    exa_expand.cpp
    split_node_disk.cpp
    exabase.cpp
    notifier.cpp)

target_link_libraries(exa_ui_common exa_os)

# Hack to get admind/src/xml_proto/xml_protocol_version.h generated.
# Theorically adding it to the sources of exa_ui_common should do the job
# (see cmake manual) but it seems it does not work across directories.
add_dependencies(exa_ui_common xml_proto)