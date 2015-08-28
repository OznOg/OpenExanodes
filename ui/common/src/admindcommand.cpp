/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/include/admindcommand.h"

#include <assert.h>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <sstream>

#include <libxml/xpath.h>

#include "common/include/exa_conversion.h"
#include "admind/src/xml_proto/xml_protocol_version.h"

using boost::lexical_cast;
using boost::shared_ptr;
using std::runtime_error;
using std::string;



std::ostream& operator <<(std::ostream &os,
			  const exa_uuid_t &uuid)
{
    exa_uuid_str_t uuid_str;
    uuid2str(&uuid, uuid_str);
    os << uuid_str;
    return os;
}


template<>
void AdmindCommand::add_param(const std::string &name,
			      const bool& value)
{
    AdmindCommand::add_param(name,
			     value ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);
}


template<>
void AdmindCommand::add_param(const std::string &name,
			      const xmlNodePtr& node)
{
    xmlNodePtr param(xmlNewChild(params, NULL, BAD_CAST("param"), NULL));
    xmlSetProp(param, BAD_CAST("name"), BAD_CAST(name.c_str()));
    xmlAddChild(param, xmlCopyNode(node, 1));
}


template<>
void AdmindCommand::add_param(const std::string &name,
			      const xmlDocPtr& doc)
{
    add_param(name, (xmlNodePtr) xmlDocGetRootElement(doc));
}


AdmindCommand::AdmindCommand(const std::string &_name,
			     const exa_uuid &cluuid):
  name(_name),
  xml_cmd(xmlNewDoc(NULL), xmlFreeDoc)
{
  assert(xml_cmd);

  xml_cmd->children = xmlNewDocNode(xml_cmd.get(), NULL, BAD_CAST("Admind"),
				    NULL);

  xmlSetProp(xml_cmd->children, BAD_CAST("protocol_release"),
	     BAD_CAST(XML_PROTOCOL_VERSION));

  if (!uuid_is_zero(&cluuid))
  {
    exa_uuid_str_t uuid_str;
    assert(name != "getconfig"
	   && name != "get_cluster_name");

    xmlNodePtr cluster = xmlNewChild(xml_cmd->children, NULL, BAD_CAST("cluster"), NULL);

    uuid2str(&cluuid, uuid_str);

    xmlSetProp(cluster, BAD_CAST("uuid"), BAD_CAST(uuid_str));
  }
  else
    assert(name == "getconfig"
	   || name == "clcreate"
	   || name == "get_cluster_name"
	   || name == "get_nodedisks"
           || name == "getlicense");

  params = xmlNewChild(xml_cmd->children, NULL, BAD_CAST("command"), NULL);

  xmlSetProp(params, BAD_CAST("name"), BAD_CAST(get_name().c_str()));
}


string AdmindCommand::get_xml_command(bool formatted) const
{
  xmlChar *dump;

  xmlDocDumpFormatMemory(xml_cmd.get(), &dump, NULL, formatted);

  string buffer(reinterpret_cast<const char*>(dump));

  /* Fix bug #4262.
   * xmlDocDumpFormatMemory() puts a trailing '\n' at
   * the end of the string. However, Admind expects "</Admind>", not
   * "</Admind>\n". As a result, we remove the trailing '\n'.
   */
  buffer.resize(buffer.length() - 1);

  xmlFree(dump);

  return buffer;
}


string AdmindCommand::get_summary() const
{
  string summary(get_name());

  assert(xml_cmd);

  xmlNodePtr cmd = xml_conf_xpath_singleton(xml_cmd.get(), "/Admind/command");

  assert(cmd);

  for (xmlNodePtr node = cmd->children;
       node;
       node = node->next)
  {
      try
      {
          /* Using a shared_ptr here provides us with exception-safety. */
          shared_ptr<xmlChar> prop;

          prop = shared_ptr<xmlChar>(xmlGetProp(node, BAD_CAST("name")), xmlFree);
          if (!prop)
              throw runtime_error("missing \"name\" property");

          string name(reinterpret_cast<const char*>(prop.get()));

          prop = shared_ptr<xmlChar>(xmlGetProp(node, BAD_CAST("value")), xmlFree);
          if (!prop)
              throw runtime_error("missing \"value\" property");

          string value(reinterpret_cast<const char*>(prop.get()));

          summary += " " + name + "=\"" + value + "\"";
      }
      catch (...)
      {
          summary += " <bad parameter>";
      }
  }

  return summary;
}


