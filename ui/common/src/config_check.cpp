/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file config_check.cpp
    Class for the CONFIG_CHECK, The Base Class for Exanodes CLI and GUI
*/

#include "ui/common/include/config_check.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_names.h"
#include "os/include/os_network.h"
#include "ui/common/include/cli_log.h"

#include <iostream>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>

using std::string;
using std::map;
using std::vector;
using boost::lexical_cast;

/*--------------------------------------------------------------------
                         CLASS config_check: implementation
  ------------------------------------------------------------------*/


/*------------------------------------------------------------------------------*/
/** \brief check the given param is valid
 *
 *  \param param_type:   the type of the param to check
 *  \param param_value:  the value of the param to check
 *  \param optional:     the value of the param to check is optional
 *                       if not optional, an error is generated if it is NULL
 *
 *       Checks:
 *        - the param value has the correct size and uses only accepted charaters
 *
 * \return EXA_ERR_DEFAULT/EXA_SUCCESS.
 */
/*------------------------------------------------------------------------------*/
exa_error_code
ConfigCheck::check_param(KindOfParam param_type, uint size,
                         const string& param_value, bool optional)
{
  exa_error_code status = EXA_SUCCESS;

  if(param_value.empty() && !optional)
    {
      exa_cli_error("%sERROR%s: One parameter is missing\n",
                    COLOR_ERROR, COLOR_NORM);
      return EXA_ERR_DEFAULT;
    }

  if(param_value.empty())	/* It's an optionnal param */
    return EXA_SUCCESS;

  string regexp, errmsg;

  switch(param_type)
    {
    case CHECK_NAME:
      regexp = EXACONFIG_EXANAME_REGEXP_EXPANDED;
      errmsg = string( "Must be in the range ") + EXACONFIG_EXANAME_REGEXP;
      break;
    case CHECK_MOUNTPOINT:
      regexp = EXACONFIG_MOUNTPOINT_REGEXP_EXPANDED;
      errmsg = string( "Must be in the range ") + EXACONFIG_MOUNTPOINT_REGEXP;
      break;
    case CHECK_DEVPATH:
      regexp = EXACONFIG_DEVPATH_REGEXP_EXPANDED;
      errmsg = string( "Must be in the range ") + EXACONFIG_DEVPATH_REGEXP;
      break;
    }

  if( param_value.find_first_not_of(regexp) != string::npos )
    {
      exa_cli_error("%sERROR%s: The parameter with value '%s' is not well formed\n"
                    "       %s\n",
                    COLOR_ERROR, COLOR_NORM,
                    param_value.c_str(),
                    errmsg.c_str());
      status = EXA_ERR_DEFAULT;
    }

  if(param_value.length() > size )
    {
      exa_cli_error("%sERROR%s: The size of the parameter with value '%s'"
                    " is greater than %d chars\n",
                    COLOR_ERROR, COLOR_NORM,
                    param_value.c_str(),
                    size);
      status = EXA_ERR_DEFAULT;
    }

  return(status);
}

/*------------------------------------------------------------------------------*/
/** \brief normalize param. It is mandatory, in order to not allocate memory
 *         on the admind side to create empty but correctly sized params
 *         everywhere we need them.
 *
 * \param aconfigDocPtr: the config tree to check
 *
 * \return EXA_ERR_DEFAULT/EXA_SUCCESS.
 */
/*------------------------------------------------------------------------------*/
void
ConfigCheck::normalize_param(xmlDocPtr configDocPtr)
{
  xmlNodePtr     node;
  int            i;

  EXA_ASSERT(configDocPtr);

  xmlNodeSetPtr node_set = xml_conf_xpath_query(configDocPtr, "//cluster/node");
  xml_conf_xpath_result_for_each(node_set, node, i)
  {
    /* Assign the node number in the tree */
    xml_set_prop(node, EXA_CONF_NODE_NUMBER, lexical_cast<string>(i).c_str());
  }
  xml_conf_xpath_free(node_set);
}


/** \brief utility function related to config file creation.
 *         insert the network node in a cluster/node xml tree
 *
 * \param xmlnode: the cluster/node to insert in
 * \param network: the network type and IP addresses.
 *
 */
void
ConfigCheck::insert_node_network(xmlNodePtr xmlnode, const string& _network)
{
    char canonical_hostname[EXA_MAXSIZE_HOSTNAME + 1];
    string node_name = xml_get_prop(xmlnode, "name");
    xmlNodePtr xmlnetwork = xml_new_child(xmlnode, NULL, "network", NULL);
    vector<string> network_list;
    string network;
    string address;

    if (os_host_canonical_name(node_name.c_str(), canonical_hostname,
                               sizeof(canonical_hostname)) != 0)
        throw ConfigException("Failed to get canonical hostname for '"
                              + node_name + "'");

    /* Go thru the list and removes empty strings (end user passsed spaces
    * ranges 'AAA   B  bbbb ' fe) see bug #3925 */
    network = boost::trim_copy(_network);

    boost::split(network_list, network, boost::algorithm::is_any_of(" "));

    if (network == "")
        xml_set_prop(xmlnetwork, "hostname", canonical_hostname);
    else if (network_list.size() == 1 &&
             network_list[0].find( ':', 0) == string::npos)
        xml_set_prop(xmlnetwork, "hostname", network.c_str());
    else
        for (vector<string>::const_iterator it = network_list.begin();
        it != network_list.end(); it++)
        {
            string node_address = *it;
            string::size_type column = node_address.find( ':', 0);

            if (column != string::npos &&
                node_address.substr(0, column) == node_name)
            {
                if (address.empty())
                    address = node_address.substr(column + 1);
                else
                    throw ConfigException("Multiple IP addresses for hostname '"
                                          + node_name + "'");
            }
            xml_set_prop(xmlnetwork, "hostname", address.c_str());
        }
}
