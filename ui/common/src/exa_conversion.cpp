/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/include/exa_conversion.hpp"
#include "common/include/exa_conversion.h"
#include <errno.h>



int64_t exa::to_int64(const std::string &str)
{
    int64_t tmp;
    int err = ::to_int64(str.c_str(), &tmp);
    switch (err)
    {
    case EXA_SUCCESS:
	return tmp;
    case -ERANGE:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to int64 (out of range)");
    case -EINVAL:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to int64 (invalid argument)");
    default:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to int64 (unknown reason)");
    }
}



uint64_t exa::to_uint64(const std::string &str)
{
    uint64_t tmp;
    int err = ::to_uint64(str.c_str(), &tmp);
    switch (err)
    {
    case EXA_SUCCESS:
	return tmp;
    case -ERANGE:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to uint64 (out of range)");
    case -EINVAL:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to uint64 (invalid argument)");
    default:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to uint64 (unknown reason)");
    }
}



int32_t exa::to_int32(const std::string &str)
{
    int32_t tmp;
    int err = ::to_int32(str.c_str(), &tmp);
    switch (err)
    {
    case EXA_SUCCESS:
	return tmp;
    case -ERANGE:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to int32 (out of range)");
    case -EINVAL:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to int32 (invalid argument)");
    default:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to int32 (unknown reason)");
    }
}



uint32_t exa::to_uint32(const std::string &str)
{
    uint32_t tmp;
    int err = ::to_uint32(str.c_str(), &tmp);
    switch (err)
    {
    case EXA_SUCCESS:
	return tmp;
    case -ERANGE:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to uint32 (out of range)");
    case -EINVAL:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to uint32 (invalid argument)");
    default:
	throw ConversionException(std::string("Cannot convert '") +
				  str + "' to uint32 (unknown reason)");
    }
}


char exa::to_lower(char c)
{
    return ::tolower(c);
}


uint64_t exa::to_size_kb(const std::string &size_string)
{
    uint64_t tmp;
    int err = ::exa_get_size_kb(size_string.c_str(), &tmp);

    if (err ==  EXA_SUCCESS)
        return tmp;

    switch (err)
    {
    case -ERANGE:
        throw ConversionException(std::string("The provided size '") +
                                  size_string + "' is out of range.");
    case -EINVAL:
        throw ConversionException(std::string("Failed to parse the provided size '") +
                                  size_string + "'.");
    default:
        throw ConversionException(std::string("Failed to parse the provided size '") +
                                  size_string + "'.");
    }
}


char * exa::to_human_size(char *size_str, size_t n, uint64_t size_kb)
{
    return ::exa_get_human_size(size_str, n, size_kb);
}
