#
# Copyright 2002, 2012 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

cmake_minimum_required(VERSION 3.15)

# --- Sanity check ---------------------------------------------------
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Please use out-of-tree compilation.")
endif()

# --- Set the default compiler ---------------------------------------
# This must remain *before* the project() command.
if(WIN32)
    find_program(CMAKE_C_COMPILER icl REQUIRED)
    find_program(CMAKE_CXX_COMPILER icl REQUIRED)
endif()

# --- Declare the project --------------------------------------------
project(exanodes LANGUAGES C CXX)

if(NOT WIN32)
    execute_process(COMMAND "date" "+%Y%m%d"
        OUTPUT_VARIABLE CURRENT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# --- Platform detection -----------------------------------------------
set(PLATFORM_TARGET "linux" CACHE STRING "Target platform: 'win', 'linux' or 'vmware'")
if(WIN32)
    set(PLATFORM_TARGET "win" CACHE STRING "Target platform: 'win', 'linux' or 'vmware'")
endif()

# Configure platform-specific variables
set(EXA_EDITION_STR "")
set(EXA_VERSION "5.0.0")
if(PLATFORM_TARGET STREQUAL "vmware")
    set(EXA_EDITION_TAG "vm")
    set(EXA_PLATFORM_STR "VMware")
    set(VMWARE TRUE CACHE BOOL "Vmware flag" FORCE)
elseif(PLATFORM_TARGET STREQUAL "win" OR WIN32)
    set(EXA_EDITION_TAG "win")
    set(EXA_PLATFORM_STR "Windows")
    set(WIN TRUE CACHE BOOL "Windows" FORCE)
elseif(PLATFORM_TARGET STREQUAL "linux")
    set(EXA_EDITION_TAG "hpc")
    set(EXA_PLATFORM_STR "Linux")
    set(LINUX TRUE CACHE BOOL "Linux" FORCE)
else()
    message(FATAL_ERROR "Invalid PLATFORM_TARGET '${PLATFORM_TARGET}' (valid targets: 'win', 'linux', 'vmware').")
endif()
set(EXA_TARBALL_NAME "${CMAKE_PROJECT_NAME}-${EXA_EDITION_TAG}-${EXA_VERSION}")

# --- Define components -----------------------------------------------
set(WITH_NODES TRUE CACHE BOOL "Enable the cluster side")
set(WITH_CLI TRUE CACHE BOOL "Enable the command line interface")
set(WITH_LIBAIO FALSE CACHE BOOL "Enable the use of libaio for performing IOs instead of Exanodes rdev kernel module")
set(WITH_DOCS FALSE CACHE BOOL "Enable the documentation")

if(WIN32)
    set(WITH_FS FALSE)
    set(WITH_MONITORING FALSE)
    set(WITH_SELINUX FALSE)
    set(WITH_BDEV FALSE)
else()
    set(WITH_FS FALSE CACHE BOOL "Enable filesystems")
    set(WITH_MONITORING FALSE CACHE BOOL "Enable the monitoring service")
    set(WITH_SELINUX FALSE CACHE BOOL "Enable the SElinux policy")
    set(WITH_BDEV TRUE CACHE BOOL "Compile the bdev target")
endif()

set(WITH_DKMS TRUE CACHE BOOL "Use dkms to build kernel modules")


set(WITH_PERF FALSE CACHE BOOL "Enable performance instrumentation")
set(WITH_UT TRUE CACHE BOOL "Build unit tests")
set(WITH_UT_ROOT TRUE CACHE BOOL "Build unit tests that requires to be root to succeed")
set(WITH_TEST TRUE CACHE BOOL "Enable the test suite")
set(WITH_TOOLS FALSE CACHE BOOL "Enable the tools")
set(WITH_MEMTRACE FALSE CACHE BOOL "Check/stat user space memory allocations")
set(WITH_SUPSIM FALSE CACHE BOOL "Make csupd standalone to allow simulations")
set(WITH_TRACE FALSE CACHE BOOL "Make the logs very verbose")

set(WITH_EXA_COMMON_KMODULE TRUE CACHE BOOL "Use exa_common kernel module")

# --- Define installation directories ---------------------------------
set(PKG_NAME "exanodes")
set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE PATH "" FORCE)
if(WIN32)
    set(CMAKE_INSTALL_PREFIX "" CACHE PATH "Installation directory prefix" FORCE)
endif()
set(BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Path to user executables [CMAKE_INSTALL_PREFIX/bin]")
file(TO_NATIVE_PATH ${BIN_DIR} NATIVE_BIN_DIR)
string(REPLACE "\\" "\\\\" NATIVE_BIN_DIR ${NATIVE_BIN_DIR})
set(SBIN_DIR "${CMAKE_INSTALL_PREFIX}/sbin" CACHE PATH "Path to system admin executables [CMAKE_INSTALL_PREFIX/sbin]")
file(TO_NATIVE_PATH ${SBIN_DIR} NATIVE_SBIN_DIR)
string(REPLACE "\\" "\\\\" NATIVE_SBIN_DIR ${NATIVE_SBIN_DIR})
set(LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Path to object code libraries [CMAKE_INSTALL_PREFIX/lib]")
set(DATA_DIR "${CMAKE_INSTALL_PREFIX}/share" CACHE PATH "Path to read-only data [CMAKE_INSTALL_PREFIX/share]")
set(PKG_LIB_DIR "${LIB_DIR}/${PKG_NAME}")
file(TO_NATIVE_PATH ${PKG_LIB_DIR} NATIVE_PKG_LIB_DIR)
string(REPLACE "\\" "\\\\" NATIVE_PKG_LIB_DIR ${NATIVE_PKG_LIB_DIR})
set(PKG_DATA_DIR "${DATA_DIR}/${PKG_NAME}")
file(TO_NATIVE_PATH ${PKG_DATA_DIR} NATIVE_PKG_DATA_DIR)
string(REPLACE "\\" "\\\\" NATIVE_PKG_DATA_DIR ${NATIVE_PKG_DATA_DIR})

if(WIN32)
    set(SYSCONF_DIR "{%PROGRAMDATA}\${PKG_NAME}\etc")
    set(PKG_CACHE_DIR "{%PROGRAMDATA}\${PKG_NAME}\cache")
    set(PKG_NODE_CONF_DIR ${SYSCONF_DIR})
    set(PKG_LOG_DIR "{%PROGRAMDATA}\${PKG_NAME}\log")
else()
    set(SYSCONF_DIR "${CMAKE_INSTALL_PREFIX}/etc" CACHE PATH "Path to config [CMAKE_INSTALL_PREFIX/etc]")
    if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
        set(PKG_ETC_DIR "/etc/")
        set(PKG_PID_DIR "/var/run" CACHE PATH "Path to pid files [/var/run]")
        set(PKG_LOG_DIR "/var/log/${PKG_NAME}" CACHE PATH "Path to log files [/var/log/${PKG_NAME}]")
        set(PKG_CACHE_DIR "/var/cache/${PKG_NAME}" CACHE PATH "Path to cache [/var/cache/${PKG_NAME}]")
    else()
        set(PKG_ETC_DIR "${CMAKE_INSTALL_PREFIX}/etc/")
        set(PKG_PID_DIR "${CMAKE_INSTALL_PREFIX}/var/run" CACHE PATH "Path to pid files [/var/run]")
        set(PKG_LOG_DIR "${CMAKE_INSTALL_PREFIX}/var/log/${PKG_NAME}" CACHE PATH "Path to log files [/var/log/${PKG_NAME}]")
        set(PKG_CACHE_DIR "${CMAKE_INSTALL_PREFIX}/var/cache/${PKG_NAME}" CACHE PATH "Path to cache [/var/cache/${PKG_NAME}]")
    endif()
    set(PKG_NODE_CONF_DIR "${PKG_ETC_DIR}/${PKG_NAME}" CACHE PATH "Path to node conf [${PKG_ETC_DIR}/${PKG_NAME}]")
    set(DKMS_DIR "${CMAKE_INSTALL_PREFIX}/src/${EXA_TARBALL_NAME}" CACHE PATH "Path to dkms source directory")
    set(MAN_DIR "${DATA_DIR}/man" CACHE PATH "Path to man documentation [DATA_DIR/man]")
    if(WITH_PERF)
        set(EXPORT_PERF_CONF_ENV "export EXANODES_PERF_CONFIG=\"${PKG_NODE_CONF_DIR}/exaperf.conf\"")
    endif()
endif()

# --- Check if valgrind is installed ---------------------------------
if(NOT WIN32)
    find_file(HAVE_VALGRIND_MEMCHECK_H valgrind/memcheck.h)
endif()

# --- Generate configuration headers ---------------------------------
# Version
configure_file(config/exa_version.h.in config/exa_version.h)

# WITH_xxx conditionals
if(WIN32)
    if(WITH_FS)
        add_compile_definitions(WITH_FS)
    endif()
    if(WITH_BDEV)
        add_compile_definitions(WITH_BDEV)
    endif()
    if(WITH_MEMTRACE)
        add_compile_definitions(WITH_MEMTRACE)
    endif()
    if(WITH_MONITORING)
        add_compile_definitions(WITH_MONITORING)
    endif()
    if(WITH_SUPSIM)
        add_compile_definitions(WITH_SUPSIM)
    endif()
    if(WITH_TRACE)
        add_compile_definitions(WITH_TRACE)
    endif()
    if(WITH_PERF)
        add_compile_definitions(WITH_PERF)
    endif()
else()
    set(KMODULE_LIST "")
    set(KERNEL_CFLAGS "-std=gnu99")
    if(NOT WITH_LIBAIO)
        list(APPEND KMODULE_LIST "krdev")
    endif()
    if(WITH_FS)
        add_compile_definitions(WITH_FS)
    endif()
    if(WITH_BDEV)
        add_compile_definitions(WITH_BDEV)
        list(APPEND KMODULE_LIST "kbdev")
    endif()
    if(WITH_MEMTRACE)
        add_compile_definitions(WITH_MEMTRACE)
    endif()
    if(WITH_MONITORING)
        add_compile_definitions(WITH_MONITORING)
    endif()
    if(WITH_SUPSIM)
        add_compile_definitions(WITH_SUPSIM)
    endif()
    if(WITH_TRACE)
        add_compile_definitions(WITH_TRACE)
    endif()
    if(WITH_PERF)
        add_compile_definitions(WITH_PERF)
        set(KERNEL_CFLAGS "${KERNEL_CFLAGS} -DWITH_PERF")
    endif()
    if(HAVE_VALGRIND_MEMCHECK_H)
        add_compile_definitions(HAVE_VALGRIND_MEMCHECK_H)
    endif()
    if(WITH_EXA_COMMON_KMODULE)
        add_compile_definitions(USE_EXA_COMMON_KMODULE)
        list(APPEND KMODULE_LIST "kcommon")
    endif()
endif()
if(WITH_FS AND NOT WITH_BDEV)
    message(FATAL_ERROR "WITH_FS needs WITH_BDEV")
endif()

# Directories
configure_file(config/pkg_data_dir.h.in config/pkg_data_dir.h)
configure_file(config/pkg_cache_dir.h.in config/pkg_cache_dir.h)
configure_file(config/pid_dir.h.in config/pid_dir.h)

# --- Set some default values ----------------------------------------
if(NOT DEFAULT_FORCED)
    set(DEFAULT_FORCED 1 CACHE INTERNAL "Force the setting of default values on first configuration")
    if(WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "/STACK:10000000 /INCREMENTAL:YES /NODEFAULTLIB:libmmd.lib /NODEFAULTLIB:libmmdd.lib" CACHE STRING "Link flags for executables" FORCE)
        set(CMAKE_C_FLAGS "/Qstd=c99 /W3 /WX /Qdiag-disable:188,869,1229 /DLIBXML_STATIC" CACHE STRING "C flags" FORCE)
        set(CMAKE_C_FLAGS_DEBUG "/MTd /Od /Zi /GZ /DDEBUG /D_DEBUG /D_CRTDBG_MAP_ALLOC" CACHE STRING "Debug C flags" FORCE)
        set(CMAKE_C_FLAGS_RELEASE "/MT /O2 /DRELEASE" CACHE STRING "Release C flags" FORCE)
        set(CMAKE_C_FLAGS_MINSIZEREL "/MT /O1" CACHE STRING "Minisize release C flags" FORCE)
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT /O2 /Zi /DRELEASE" CACHE STRING "Release with debug C flags" FORCE)
        set(CMAKE_C_FLAGS_COVERAGE "/MT /Od /Qprof-gen:srcpos" CACHE STRING "Coverage C flags" FORCE)
        set(CMAKE_CXX_FLAGS "/W3 /WX /EHsc /DLIBXML_STATIC" CACHE STRING "CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd /Od /Zi /GZ /EHsc /DDEBUG /D_DEBUG /D_CRTDBG_MAP_ALLOC" CACHE STRING "Debug CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /EHsc /DRELEASE" CACHE STRING "Release CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_MINSIZEREL "/MT /O1 /EHsc" CACHE STRING "Minisize release CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /O2 /Zi /EHsc /DRELEASE" CACHE STRING "Release with debug CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_COVERAGE "/MT /Od /Qprof-gen:srcpos /EHsc" CACHE STRING "Coverage CXX flags" FORCE)
    else()
        set(CMAKE_C_FLAGS "-Wall -fno-strict-aliasing -Werror -Wno-error=unused-parameter -Wdeclaration-after-statement" CACHE STRING "C flags" FORCE)
        set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb -DDEBUG" CACHE STRING "Debug C flags" FORCE)
        set(CMAKE_C_FLAGS_RELEASE "-O2 -ggdb -DRELEASE -D_FORTIFY_SOURCE=2" CACHE STRING "Release C flags" FORCE)
        set(CMAKE_C_FLAGS_MINSIZEREL "-Os -D_FORTIFY_SOURCE=2" CACHE STRING "Minisize release C flags" FORCE)
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O0 -ggdb -DRELEASE -D_FORTIFY_SOURCE=2" CACHE STRING "Release with debug C flags" FORCE)
        set(CMAKE_C_FLAGS_COVERAGE "-ggdb -fprofile-arcs -ftest-coverage" CACHE STRING "Coverage C flags" FORCE)
        set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -fno-strict-aliasing -Wno-error=unused-parameter -Werror" CACHE STRING "CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -DDEBUG" CACHE STRING "Debug CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -ggdb -DRELEASE" CACHE STRING "Release CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os" CACHE STRING "Minisize release CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 -ggdb -DRELEASE" CACHE STRING "Release with debug CXX flags" FORCE)
        set(CMAKE_CXX_FLAGS_COVERAGE "-ggdb -fprofile-arcs -ftest-coverage" CACHE STRING "Coverage CXX flags" FORCE)
    endif()
endif()
mark_as_advanced(CMAKE_CXX_FLAGS_COVERAGE CMAKE_C_FLAGS_COVERAGE)

# --- Definitions -----------------------------------------------------
if(WIN32)
    add_compile_definitions(WIN32 _WIN32_WINNT=0x0600 _CRT_SECURE_NO_DEPRECATE)
    add_compile_definitions("__attribute__(x)=" __thread=__declspec(thread))
else()
    add_compile_definitions(_FILE_OFFSET_BITS=64 _GNU_SOURCE __STDC_FORMAT_MACROS __STDC_LIMIT_MACROS)
endif()

# --- Check Linux sources ---------------------------------------------
if(NOT WITH_DKMS AND NOT WIN32)
    execute_process(COMMAND uname -r
        COMMAND tr -d '\n'
        RESULT_VARIABLE UNAME_R_RESULT
        OUTPUT_VARIABLE UNAME_R_OUTPUT)
    if(UNAME_R_RESULT EQUAL 0)
        set(LINUX_VERSION ${UNAME_R_OUTPUT} CACHE STRING "Linux version")
        message(STATUS "Found Linux version ${LINUX_VERSION}")
    else()
        message(FATAL_ERROR "Linux version not found")
    endif()
    set(LINUX_DIR /lib/modules/${LINUX_VERSION}/build CACHE PATH "Linux source directory")
    if(EXISTS ${LINUX_DIR}/Makefile)
        message(STATUS "Found Linux source directory ${LINUX_DIR}")
    else()
        message(FATAL_ERROR "Linux source directory not found")
    endif()
    find_file(UTSRELEASE utsrelease.h HINTS ${LINUX_DIR}/include/*/)
    if(UTSRELEASE)
        message(STATUS "Linux is prepared to build external modules")
    else()
        message(FATAL_ERROR "The kernel is not prepared to build external modules")
    endif()
endif()

# --- Check architecture ---------------------------------------------
if(WIN32)
    execute_process(COMMAND ${CMAKE_C_COMPILER} /QV
        ERROR_VARIABLE ARCH_OUTPUT)
    if(ARCH_OUTPUT MATCHES "IA-32")
        set(ARCH 32)
    else()
        set(ARCH 64)
    endif()
    message(STATUS "Architecture: ${ARCH} bits")
endif()

# --- Check libraries -------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})
if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Program Files/boost")
endif()
if(WIN32)
    set(LIBXML2_INCLUDE_DIR "C:/Program Files/libxml2/include")
    set(LIBXML2_LIBRARIES "C:/Program Files/libxml2/lib/MT${ARCH}/libxml2_a.lib")
else()
    find_package(LibXml2 2.6 REQUIRED)
endif()
if(WITH_CLI)
    find_package(Boost 1.33.1 REQUIRED)
endif()
find_package(OpenSSL 0.9.8 REQUIRED)
if(WITH_MONITORING)
    find_package(NetSnmp REQUIRED)
endif()
if(WITH_SELINUX)
    find_package(SELinux REQUIRED)
endif()
if(NOT WIN32)
    set(LIBRT rt)
    set(LIBPTHREAD pthread)
    set(LIBDL dl)
endif()
if(WIN32)
    set(LIBWS2_32 ws2_32)
endif()

# --- Check programs --------------------------------------------------
if(WITH_DOCS)
    find_program(XSLTPROC xsltproc "C:/Program Files/libxslt/bin")
    find_program(FOP fop "C:/Program Files/fop")
endif()

# --- Unit testing ----------------------------------------------------
if(WITH_UT)
    find_program(UT_BUILD ut_build "${CMAKE_SOURCE_DIR}/unit_testing/")
    if(NOT UT_BUILD)
        message(FATAL_ERROR "Seanodes' unit testing library is not installed.")
    endif()
endif()
set(UT_HEADER_DIRECTORY "${CMAKE_SOURCE_DIR}/unit_testing")

enable_testing()

# --- Autogenerated header that contains version and git revision ---
add_custom_target(git.h
    ALL
    COMMAND perl ${CMAKE_SOURCE_DIR}/tools/generate_git_h.pl ${CMAKE_SOURCE_DIR}
    COMMENT "Generate git.h"
)

# --- Includes directories --------------------------------------------
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})
include_directories(${UT_HEADER_DIRECTORY})
include_directories(${LIBXML2_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})
if(WIN32)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# --- Subdirectories --------------------------------------------------
add_subdirectory(os)
add_subdirectory(common)
add_subdirectory(admind)
add_subdirectory(tools)
if(WITH_NODES)
    add_subdirectory(log)
    if(WITH_PERF)
        add_subdirectory(exaperf)
    endif()
    add_subdirectory(examsg)
    add_subdirectory(examsgd)
    add_subdirectory(csupd)
    add_subdirectory(blockdevice)
    add_subdirectory(target)
    add_subdirectory(lum)
    add_subdirectory(vrt)
    add_subdirectory(rdev)
    add_subdirectory(nbd)
    if(WITH_MONITORING)
        add_subdirectory(monitoring)
    endif()
    if(WITH_FS)
        add_subdirectory(fs)
    endif()
endif()
if(WITH_NODES AND WITH_DKMS)
    if(WITH_BDEV OR WITH_EXA_COMMON_KMODULE OR NOT WITH_LIBAIO)
        add_subdirectory(dkms)
    endif()
endif()
add_subdirectory(sysconfig)
if(WITH_CLI)
    add_subdirectory(pkgdata)
    add_subdirectory(ui)
endif()
add_subdirectory(token_manager)
if(WITH_DOCS)
    add_subdirectory(docs)
endif()

# --- Packaging -------------------------------------------------------
if(WIN32)
    get_filename_component(WIX_PATH
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\MICROSOFT\\Windows\\ Installer\\ XML\\3.0;InstallRoot]
        ABSOLUTE)
    if(NOT EXISTS "${WIX_PATH}/candle.exe")
        message(FATAL_ERROR "Could not find Wix tools in ${WIX_PATH}")
    endif()
    if(WITH_TOOLS)
        set(EMBED_TOOLS 1)
    else()
        set(EMBED_TOOLS 0)
    endif()
    if(WITH_CLI)
        set(EMBED_CLI 1)
    else()
        set(EMBED_CLI 0)
    endif()
    if(WITH_PERF)
        set(EMBED_PERF 1)
    else()
        set(EMBED_PERF 0)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(EMBED_PDB 1)
    else()
        set(EMBED_PDB 0)
    endif()
    include("windows/exanodes-guid-wxs.cmake")
    configure_file("exanodes.wxs.in" "${CMAKE_CURRENT_BINARY_DIR}/exanodes.wxs" @ONLY)
    configure_file("token_manager.wxs.in" "${CMAKE_CURRENT_BINARY_DIR}/token_manager.wxs" @ONLY)
    add_custom_target(dist
        COMMAND ${CMAKE_MAKE_PROGRAM} install DESTDIR=install
        COMMAND ${WIX_PATH}/candle.exe -arch x64 -ss exanodes.wxs -dEMBED_PDB=${EMBED_PDB} -dEMBED_TOOLS=${EMBED_TOOLS} -dEMBED_PERF=${EMBED_PERF}
        COMMAND ${WIX_PATH}/light.exe -ss exanodes.wixobj -ext WixUIExtension -out ${CMAKE_CURRENT_BINARY_DIR}/${EXA_TARBALL_NAME}.msi
        COMMENT "Generate ${EXA_TARBALL_NAME}.msi"
    )
else()
    configure_file("${CMAKE_PROJECT_NAME}.spec.in" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.spec" @ONLY)
    file(RELATIVE_PATH RELATIVE_BINARY_DIR ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    add_custom_target(dist
        DEPENDS git.h
        COMMAND perl ${CMAKE_SOURCE_DIR}/tools/update_debian_changelog.pl ${CMAKE_SOURCE_DIR}
        COMMAND mkdir -p ${EXA_TARBALL_NAME}
        COMMAND cp -a -u -f ${CMAKE_SOURCE_DIR}/* ./git.h ./${CMAKE_PROJECT_NAME}.spec ${EXA_TARBALL_NAME}
        COMMAND tar cz --exclude ${RELATIVE_BINARY_DIR} --exclude exanodes-policy-no-iobarriers.spec --exclude exanodes-policy.spec --exclude .git --exclude '*~' ${EXA_TARBALL_NAME} > ${EXA_TARBALL_NAME}.tar.gz
        COMMENT "Generate ${EXA_TARBALL_NAME}.tar.gz"
    )
endif()

# --- Doxygen ---------------------------------------------------------
configure_file(Doxyfile.in Doxyfile @ONLY)
add_custom_target(doxygen
    COMMAND rm -rf doxygen
    COMMAND mkdir -p doxygen
    COMMAND doxygen
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# --- ctags/etags -----------------------------------------------------
if(NOT WIN32)
    find_program(CTAGS ctags)
    find_program(ETAGS etags)
    add_custom_target(tags
        COMMAND rm -f ${CMAKE_BINARY_DIR}/tags
        COMMAND find ${CMAKE_SOURCE_DIR} \( -name '*.c' -o -name '*.h' -o -name '*.cpp' \) -exec ${CTAGS} -a -o ${CMAKE_BINARY_DIR}/tags {} ";"
        COMMAND ${ETAGS} --declarations -o ${CMAKE_BINARY_DIR}/TAGS `find ${CMAKE_SOURCE_DIR} -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.h'`)
endif()
