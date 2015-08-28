#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_MONITORING)
    add_subdirectory(agentx)
    add_subdirectory(common)
    add_subdirectory(md_com)
    add_subdirectory(md)
    add_subdirectory(md_client)
endif (WITH_MONITORING)

