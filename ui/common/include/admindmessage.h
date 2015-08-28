/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADMINDMESSAGE_H__
#define __ADMINDMESSAGE_H__

#include <libxml/tree.h>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

#include "common/include/exa_error.h"


class AdmindMessage
{
   static const std::string DONE_TAG;
   static const std::string INPROGRESS_TAG;
   static const std::string PAYLOAD_TAG;

public:
  typedef enum MessageType {
    DONE,
    INPROGRESS,
    PAYLOAD
  } MessageType;

  /* This method throws an exception in case of a parsing error. */
  explicit AdmindMessage(const std::string &_command_name,
                         const std::string &_connected_node,
			 const std::string &_xml_msg);

  MessageType get_type() const
  {
    return type;
  }
  const std::string &get_description() const
  {
    return description;
  }

  const std::string get_payload() const
  {
    return payload_string;
  }

  void set_payload(const std::string &str)
  {
    payload_string = str;
  }

  const exa_error_code &get_error_code() const
  {
    return error_code;
  }
  const std::string &get_error_msg() const
  {
    return error_msg;
  }
  const std::string &get_error_node() const
  {
    return error_node;
  }
  const std::string &get_connected_node() const
  {
    return connected_node;
  }

  boost::shared_ptr<xmlDoc> get_subtree() const;
  std::string get_summary() const;
  std::string dump() const;

private:
  /* I would have preferred to have many of these members public (and
   * const!), avoiding the accesoors, but because they are initialized
   * in the constructor body rather than in the initializer list (and
   * so, cannot be const), things have to be this way. Oh well. */
  boost::shared_ptr<xmlDoc> xml_msg;
  std::string payload_string;
  xmlNodePtr subtree;
  MessageType type;
  std::string result_name;
  std::string description;
  exa_error_code error_code;
  std::string error_msg;
  std::string error_node;
  std::string command_name;
  std::string connected_node;
};


#endif /* __ADMINDMESSAGE_H__ */
