/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CONVERSION_HPP__
#define __EXA_CONVERSION_HPP__


#include "ui/common/include/exception.hpp"

#include "os/include/os_inttypes.h"

namespace exa
{

EXA_BASIC_EXCEPTION_DECL(ConversionException, Exception);

int64_t to_int64(const std::string &str);
uint64_t to_uint64(const std::string &str);

int32_t to_int32(const std::string &str);
uint32_t to_uint32(const std::string &str);

char to_lower(char c);

uint64_t to_size_kb(const std::string &size_string);
char * to_human_size(char *size_str, size_t n, uint64_t size_kb);

}




#endif /* __EXA_CONVERSION_HPP__ */
