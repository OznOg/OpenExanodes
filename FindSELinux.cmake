#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#


if (SELINUX_MAKEFILE)
# Already in cache, be silent
    set(NetSnmp_FIND_QUIETLY TRUE)
endif (SELINUX_MAKEFILE)

find_file(SELINUX_MAKEFILE selinux/devel/Makefile
    PATH /usr/share/)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SELinux
    DEFAULT_MSG
    SELINUX_MAKEFILE
)

mark_as_advanced(
    SELINUX_MAKEFILE)

execute_process(COMMAND  bash -c "sestatus -v | grep -i 'policy version' | awk '{ print $NF }'" OUTPUT_VARIABLE SELINUX_POLICY_VERSION)

message("Detected SELinux policy version " ${SELINUX_POLICY_VERSION})
