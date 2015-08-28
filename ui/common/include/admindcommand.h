/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADMINDCOMMAND_H__
#define __ADMINDCOMMAND_H__

#include <libxml/tree.h>

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/uuid.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <sstream>
#include <stdexcept>


std::ostream& operator <<(std::ostream &os,
			  const exa_uuid_t &uuid);


class AdmindCommand: private boost::noncopyable
{
private:
    const std::string name;
    boost::shared_ptr<xmlDoc> xml_cmd;
    xmlNodePtr params;

public:

  AdmindCommand(const std::string &name, const exa_uuid &cluuid);

  template<class E>
      void add_param(const std::string &name,
		     const E& value);

  template<class E>
      void replace_param(const std::string &name,
			 const E& value);

  std::string get_xml_command(bool formatted = false) const;
  std::string get_summary() const;

  const std::string &get_name() const { return name; };

};



template<class E>
void AdmindCommand::add_param(const std::string &name,
			      const E& value)
{
    xmlNodePtr param(xmlNewChild(params, NULL, BAD_CAST("param"), NULL));
    std::stringstream ss;
    ss << value;
    xmlSetProp(param, BAD_CAST("name"), BAD_CAST(name.c_str()));
    xmlSetProp(param, BAD_CAST("value"), BAD_CAST(ss.str().c_str()));
}


template<>
void AdmindCommand::add_param(const std::string &name,
                              const bool& value);


template<>
void AdmindCommand::add_param(const std::string &name,
                              const xmlNodePtr& node);


template<>
void AdmindCommand::add_param(const std::string &name,
                              const xmlDocPtr& doc);


template<class E>
void AdmindCommand::replace_param(const std::string &name,
				  const E& value)
{
    xmlNodeSetPtr set =
	xml_conf_xpath_query(xml_cmd.get(), "/Admind/command/param[@name='%s']",
			       name.c_str());
    xmlNodePtr p = (set ? xml_conf_xpath_result_entity_get(set, 0) : NULL);

    if (p)
    {
	if (xml_conf_xpath_result_entity_count(set) > 1)
	    throw std::runtime_error("parameter '" + name + "' is not unique");

	std::stringstream ss;
	ss << value;
	xmlSetProp(p, BAD_CAST("value"), BAD_CAST(ss.str().c_str()));
    }
    else
	add_param(name, value);

    xml_conf_xpath_free(set);
}


#endif /* __ADMINDCOMMAND_H__ */
