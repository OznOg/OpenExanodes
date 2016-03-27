/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/include/admindmessage.h"

#include <boost/lexical_cast.hpp>
#include <stdexcept>

#include "ui/common/include/exa_conversion.hpp"

#include "common/include/exa_config.h"
#include "admind/src/xml_proto/xml_protocol_version.h"

#include <memory>

using boost::lexical_cast;
using std::shared_ptr;
using std::exception;
using std::runtime_error;
using std::string;

const std::string AdmindMessage::DONE_TAG("DONE");
const std::string AdmindMessage::INPROGRESS_TAG("IN PROGRESS");
const std::string AdmindMessage::PAYLOAD_TAG("PAYLOAD");

static string get_xml_prop(xmlNodePtr node, const string &name)
{
  shared_ptr<xmlChar> xmlstr(xmlGetProp(node, BAD_CAST(name.c_str())),
			     xmlFree);

  if (!xmlstr)
    throw runtime_error("Exanodes protocol error: property '" + name
			+ "' not found");

  return reinterpret_cast<const char*>(xmlstr.get());
}

AdmindMessage::AdmindMessage(const std::string &_command_name,
			     const std::string &_connected_node,
			     const std::string &_xml_msg):
  xml_msg(
    xmlReadMemory(_xml_msg.data(), _xml_msg.length(), NULL, NULL,
		  XML_PARSE_NOBLANKS|XML_PARSE_NOERROR|XML_PARSE_NOWARNING),
    xmlFreeDoc),
  subtree(NULL),
  command_name(_command_name),
  connected_node(_connected_node)
{
  if (!xml_msg)
    throw runtime_error("Failed to parse admind message '" + _xml_msg + "'");

  shared_ptr<xmlNodeSet> set(xml_conf_xpath_query(xml_msg.get(),
						    "/Admind"),
			       xmlXPathFreeNodeSet);

  string protocol_release;
  try
  {
    protocol_release = get_xml_prop(xmlXPathNodeSetItem(set, 0),
	                          "protocol_release");
  }
  catch (...)
  {
    throw runtime_error("Exanodes protocol error: 'protocol_release' tag not found");
  }

  if (string(lexical_cast<string>(XML_PROTOCOL_VERSION)) != protocol_release)
    {
      type = DONE;
      error_code = EXA_ERR_BAD_PROTOCOL;
      error_msg = exa_error_msg(error_code);
      return;
    }


  set = shared_ptr<xmlNodeSet>(xml_conf_xpath_query(xml_msg.get(),
						  "/Admind/result"),
			     xmlXPathFreeNodeSet);

  if (!set)
    throw runtime_error("Exanodes protocol error: 'result' tag not found");

  if (xmlXPathNodeSetGetLength(set.get()) != 1)
    throw runtime_error("Exanodes protocol error: More than one 'result' "
			"tag found");

  string type_tag = lexical_cast<string>(get_xml_prop(xmlXPathNodeSetItem(set, 0), "type"));

  if (type_tag == DONE_TAG)
      type = DONE;
  else if (type_tag == INPROGRESS_TAG)
      type = INPROGRESS;
  else if (type_tag == PAYLOAD_TAG)
      type = PAYLOAD;
  else
    throw runtime_error("Exanodes protocol error: unknown message type");

  set = shared_ptr<xmlNodeSet>(xml_conf_xpath_query(xml_msg.get(),
	"/Admind/result/error"),
      xmlXPathFreeNodeSet);

  if (type == DONE || type == INPROGRESS)
    {
      if (!set)
	throw runtime_error("Exanodes protocol error: 'error' tag not found");

      error_code = static_cast<exa_error_code>(
	  -exa::to_int32(get_xml_prop(xmlXPathNodeSetItem(set, 0), "code").c_str()));

      if (error_code < 0)
	throw runtime_error("Error code must be negative or 0 (received "
	    + lexical_cast<string>(-error_code) + ")");

      error_msg = get_xml_prop(xmlXPathNodeSetItem(set, 0), "message");
      if (error_msg.empty())
	error_msg = exa_error_msg(error_code);

      if (type == INPROGRESS)
	{
	  error_node = get_xml_prop(xmlXPathNodeSetItem(set, 0), "node");
	  description = get_xml_prop(xmlXPathNodeSetItem(set, 0), "description");
	}
    }

  if (type == PAYLOAD)
    {
      set = shared_ptr<xmlNodeSet>(xml_conf_xpath_query(
	    xml_msg.get(),
	    "/Admind/result/child::node()[name()!='error']"),
	  xmlXPathFreeNodeSet);

      if (xmlXPathNodeSetGetLength(set) != 1)
	throw runtime_error("Exanodes protocol error: more than one subtree found");

      subtree = xmlXPathNodeSetItem(set, 0);
    }
}


shared_ptr<xmlDoc> AdmindMessage::get_subtree() const
{
  shared_ptr<xmlDoc> subtreedoc;

  if (subtree)
  {
    subtreedoc = shared_ptr<xmlDoc>(xmlNewDoc(NULL), xmlFreeDoc);

    xmlDocSetRootElement(subtreedoc.get(), xmlCopyNode(subtree, 1));
  }

  return subtreedoc;
}

string AdmindMessage::get_summary() const
{
  string summary(command_name);

  switch (type)
    {
      case INPROGRESS:
	summary += string(" in progress ");
	break;
      case PAYLOAD:
	summary += string(" progressive payload ");
	break;
      case DONE:
	summary += string(" completed ");
	break;
    }

  summary += string("on ") + connected_node;
  summary += string(":") + result_name;
  summary += string(" error_code=" + lexical_cast<string>(error_code));
  summary += string(" msg='") + error_msg + "'";

  if (!description.empty())
    summary += " description=\"" + description + "\"";

  if (!error_node.empty())
    summary += " node=\"" + error_node + "\"";

  return summary;
}


string AdmindMessage::dump() const
{
  xmlChar *dump;

  xmlDocDumpFormatMemory(xml_msg.get(), &dump, NULL, true);

  string buffer(reinterpret_cast<const char*>(dump));

  xmlFree(dump);

  return buffer;
}


