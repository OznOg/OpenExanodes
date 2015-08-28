/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXCEPTION_HPP__
#define __EXCEPTION_HPP__

#include <exception>
#include <string>
#include <common/include/exa_error.h>

namespace exa
{

class Exception : public std::exception
{
public:
    Exception(exa_error_code error_code) : _message (""), _error_code(error_code) {}
    Exception(const std::string &message, exa_error_code error_code = EXA_ERR_DEFAULT) : _message (message), _error_code(error_code) {}
    virtual ~Exception() throw () {}
    virtual const char * what() const throw () { return _message.c_str (); }
    virtual exa_error_code error_code() const throw () { return _error_code; }
private:
    const std::string _message;
    exa_error_code _error_code;
};

}


#define EXA_BASIC_EXCEPTION_DECL(name, base)				\
    class name : public base							\
    {									\
    public:								\
	name(exa_error_code error_code) : base(error_code) {}		\
	name(const std::string& message, exa_error_code error_code = EXA_ERR_DEFAULT) : base(message, error_code) {}		\
	~name() throw () {}							\
    }



#endif /* __EXCEPTION_HPP__ */
