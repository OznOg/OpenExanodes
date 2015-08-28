#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(exa_os_common STATIC
    os_file.c
    os_network.c
    os_time.c
    os_string.c)

set_target_properties(exa_os_common PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(exa_os_common exa_os)
