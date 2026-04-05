#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if(WITH_NODES)
    add_subdirectory(init.d)
    add_subdirectory(exanodes)
    add_subdirectory(logrotate.d)
    add_subdirectory(udev)
endif()

if(WITH_SELINUX)
    add_subdirectory(selinux/targeted)
endif()
