/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By downloading, copying, installing or
 * using the software you agree to this license. If you do not agree to this license, do not download, install,
 * copy or use the software.
 *
 * Intel License Agreement
 *
 * Copyright (c) 2000, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * -Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *  following disclaimer.
 *
 * -Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *  following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * -The name of Intel Corporation may not be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ENDIANNESS_H_
#define _ENDIANNESS_H_

/*
 * Byte Order
 */
#ifdef WIN32
#pragma warning(push, disable: 810)
#endif

#include "os/include/os_inttypes.h"

static inline void set_bigendian16(uint16_t val, unsigned char *bigendian16)
{
    bigendian16[1] = (unsigned char)(val & 0xff);
    bigendian16[0] = (unsigned char)(val >> 8);
    /* assert((val >> 16)==0); */
}


static inline void set_bigendian32(uint32_t val, unsigned char *bigendian32)
{
    bigendian32[3] = (unsigned char)(val & 0xff);
    bigendian32[2] = (unsigned char)((val >> 8) & 0xff);
    bigendian32[1] = (unsigned char)((val >> 16) & 0xff);
    bigendian32[0] = (unsigned char)((val >> 24) & 0xff);
    /* assert((val >> 32)==0); */
}


static inline void set_bigendian64(uint64_t val, unsigned char *bigendian64)
{
    bigendian64[7] = (unsigned char)(val & 0xff);
    bigendian64[6] = (unsigned char)((val >> 8) & 0xff);
    bigendian64[5] = (unsigned char)((val >> 16) & 0xff);
    bigendian64[4] = (unsigned char)((val >> 24) & 0xff);
    bigendian64[3] = (unsigned char)((val >> 32) & 0xff);
    bigendian64[2] = (unsigned char)((val >> 40) & 0xff);
    bigendian64[1] = (unsigned char)((val >> 48) & 0xff);
    bigendian64[0] = (unsigned char)((val >> 56) & 0xff);
}


#ifdef WIN32
#pragma warning(pop)
#endif

static inline uint16_t get_bigendian16(const unsigned char *bigendian16)
{
    return bigendian16[1] + (bigendian16[0] << 8);
}


static inline uint32_t get_bigendian32(const unsigned char *bigendian32)
{
    return ((uint32_t) get_bigendian16(bigendian32)) << 16 |
           ((uint32_t) get_bigendian16(bigendian32 + 2));
}


static inline uint64_t get_bigendian64(const unsigned char *bigendian64)
{
    return ((uint64_t) get_bigendian32(bigendian64)) << 32 |
           ((uint64_t) get_bigendian32(bigendian64 + 4));
}

#endif /* _UTIL_H_ */
