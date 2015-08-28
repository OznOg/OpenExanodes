# - Try to find the OpenSSL encryption library
# Once done this will define
#
#  OPENSSL_FOUND - system has the OpenSSL library
#  OPENSSL_INCLUDE_DIR - the OpenSSL include directory
#  OPENSSL_LIBRARIES - The libraries needed to use OpenSSL

# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

FIND_PATH(OPENSSL_INCLUDE_DIR
    NAMES openssl/ssl.h
    PATHS
    "$ENV{ProgramW6432}/Seanodes/OpenSSL/include/"
    "$ENV{ProgramFiles}/Seanodes/OpenSSL/include/"
)

IF(WIN32)
   FIND_LIBRARY(SSL_EAY
       NAMES ssleay32
       PATHS
       "$ENV{ProgramW6432}/Seanodes/OpenSSL/lib/"
       "$ENV{ProgramFiles}/Seanodes/OpenSSL/lib/"
   )

   FIND_LIBRARY(OPENSSL_LIBEAY_LIBRARY
       NAMES libeay32
       PATHS
       "$ENV{ProgramW6432}/Seanodes/OpenSSL/lib/"
       "$ENV{ProgramFiles}/Seanodes/OpenSSL/lib/"
   )

   SET(OPENSSL_LIBRARIES ${SSL_EAY} ${OPENSSL_LIBEAY_LIBRARY})
ELSE(WIN32)
   FIND_LIBRARY(OPENSSL_LIBRARIES NAMES ssl ssleay32 ssleay32MD )
ENDIF(WIN32)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenSSL DEFAULT_MSG OPENSSL_LIBRARIES OPENSSL_INCLUDE_DIR)

MARK_AS_ADVANCED(OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES)

