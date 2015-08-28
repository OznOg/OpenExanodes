#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# Set 'var' to the compilation commandline (compiler, preprocessor definitions
# and header directories). 'lang' may be C or CXX.
function(get_compile_commandline lang var)
    get_directory_property(include_directories INCLUDE_DIRECTORIES)
    get_directory_property(compile_definitions COMPILE_DEFINITIONS)

    if (lang STREQUAL C)
        set(cmdline ${CMAKE_C_COMPILER})
        set(cmdline ${cmdline} ${CMAKE_C_COMPILER_ARG1})
        set(flags ${CMAKE_C_FLAGS})
    else (lang STREQUAL C) # CXX
        set(cmdline ${CMAKE_CXX_COMPILER})
        set(cmdline ${cmdline} ${CMAKE_CXX_COMPILER_ARG1})
        set(flags ${CMAKE_CXX_FLAGS})
    endif (lang STREQUAL C)

    separate_arguments(flags)

    foreach (flag ${flags})
        set(cmdline ${cmdline} ${flag})
    endforeach (flag ${flags})

    foreach (def ${compile_definitions})
        set(cmdline ${cmdline} -D${def})
    endforeach (def ${compile_definitions})

    foreach (DIR ${include_directories})
        set(cmdline ${cmdline} -I${DIR})
    endforeach (DIR ${include_directories})

    set(cmdline ${cmdline} -I${UT_HEADER_DIRECTORY})
    set(${var} ${cmdline} PARENT_SCOPE)
endfunction(get_compile_commandline)
