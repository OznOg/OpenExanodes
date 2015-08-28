/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file config_check.h
    Class for the CONFIG_CHECK, containing methods for parsing the parameters
*/
#ifndef  __CONFIG_CHECK_H__
#define  __CONFIG_CHECK_H__

#include <libxml/tree.h>

#include <string>
#include <map>

#include "common/include/exa_error.h"
#include "os/include/os_inttypes.h"

class ConfigCheck;

/*--------------------------------------------------------------------
                         CLASS config_check: declaration
  ------------------------------------------------------------------*/
class ConfigCheck
{
public:
    enum KindOfParam
    {
        CHECK_NAME,
        CHECK_MOUNTPOINT,
        CHECK_DEVPATH,
    };

public:
    static exa_error_code
    check_param(KindOfParam param_type, uint size,
                const std::string& param_value, bool optional);

    static void
    insert_node_network(xmlNodePtr xmlnode, const std::string& network);

    /* Param checking */
    static void
    normalize_param(xmlDocPtr configDocPtr);
};



class ConfigException : public std::exception
{
public:
    ConfigException (const std::string& message) : _message (message) {}
    ~ConfigException () throw () {}
    virtual const char * what () const throw () { return _message.c_str (); }
private:
    const std::string _message;
};



#endif  // __CONFIG_CHECK_H__
