#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# FIXME: add -avoid-version option when building modules

# Common library

add_library(nbd_tcp STATIC
    nbd_tcp.c)

target_link_libraries(nbd_tcp
    exa_common_user
    exa_os)
