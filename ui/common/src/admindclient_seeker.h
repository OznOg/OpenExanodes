/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADMINDCLIENT_SEEKER_H__
#define __ADMINDCLIENT_SEEKER_H__

#include <map>

#include "ui/common/include/admindclient.h"
#include "ui/common/include/notifier.h"
#include "ui/common/src/admindclient_request.h"

class AdmindClient::Seeker: public Request
{
public:
  Seeker(AdmindClient &_client,
         const std::string &_cmd_name,
         const std::string &_xml_cmd,
	 std::set<std::string> &hostnames, MessageFunc _inprogress,
	 MessageFunc _progressive_payload,
	 MessageFunc _done, WarningFunc _warning, ErrorFunc _error,
	 unsigned int _timeout);
  ~Seeker();

private:
  void handle_connect(const std::string &hostname, int fd);
  void handle_done(const AdmindMessage &message, const std::string &hostname);
  void handle_error(const std::string &info, const std::string &hostname);

  void connect(const std::string &hostname);
  void do_done(const AdmindMessage &message);
  void do_error(const std::string &info);
  void do_timeout();
  void do_warning(const std::string &hostname, const std::string &info);
  void done_request();
  void maybe_try();
  void terminate();

  AdmindClient &client;
  const std::string command_name;
  const std::string xml_cmd;
  const MessageFunc inprogress;
  const MessageFunc progressive_payload;
  const MessageFunc done;
  const WarningFunc warning;
  const ErrorFunc error;

  boost::shared_ptr<Notifier::Timer> timeout;
  std::map<std::string, int> connecting;
  std::map<std::string, int> connected;
  boost::shared_ptr<RequestImpl> request;
  bool finished;
  int err;
};


#endif /* __ADMINDCLIENT_SEEKER_H__ */
