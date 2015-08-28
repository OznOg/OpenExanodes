#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_subdirectory(lib)

if (WITH_UT)
    add_subdirectory(test)
endif (WITH_UT)

if (WITH_CLI AND WITH_TOOLS)
    add_subdirectory(perl)
endif (WITH_CLI AND WITH_TOOLS)
