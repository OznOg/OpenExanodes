#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WIN32)
    add_subdirectory(src-windows)
elseif (UNIX)
    add_subdirectory(src-linux)
else (UNKNOWN)
    message(FATAL_ERROR "Unknown target system")
endif (WIN32)
