/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADMINDCLIENT_H__
#define __ADMINDCLIENT_H__

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <set>

class AdmindCommand;
class AdmindMessage;
class Notifier;


class AdmindClient: private boost::noncopyable
{
public:
  AdmindClient(Notifier &_notifier);

  typedef boost::function<void(const AdmindMessage& message)> MessageFunc;
  typedef boost::function<void(const std::string &info)> ErrorFunc;
  typedef boost::function<void(const std::string &hostname,
			       const std::string &info, int warn)
    > WarningFunc;

  class Request;

  /*
   * Takes the "command" and tries to connect to the node specified by
   * "hostname". It will then call zero or more time the "inprogress"
   * function, and then called ONE of "done" or "error", depending on
   * whether it received a response or not from exa_admind. After
   * "timeout" milliseconds, the request will be aborted.
   *
   * Note that even an error response from exa_admind constitutes a
   * response nonetheless, and thus, the "done" function will be
   * called, not the "error" function. The "error" function applies to
   * protocol errors (DNS error, socket error, timeout, malformed
   * response), rather than the higher-level errors sent using the
   * protocol.
   *
   */
  void send_node(const AdmindCommand &command,
				     const std::string &hostname,
				     MessageFunc inprogress,
				     MessageFunc progressive_payload,
				     MessageFunc done,
				     ErrorFunc error,
				     unsigned int timeout = 0);

  /*
   * This is similar to AdmindClient::send_node, but to send the
   * "command" to the leader.
   *
   * Mainly, this means that an ADMIND_ERR_NOTLEADER response will NOT
   * result in the "done" function being called, but instead,
   * AdmindClient will continue looking for the leader.
   *
   * In order to do so, it takes a set of hostnames (the "hostnames"
   * parameter) instead of a single hostname. It will attempt sending
   * the command to each host in turn, and if the set of hostnames is
   * exhausted without having been able to receive a response, the
   * "error" function will be called.
   *
   * While it only sends the command to a single host at a time,
   * AdmindClient will actually open the connection to all the hosts
   * simultaneously, sending the command to the first host that
   * responds. This minimizes the latency in case of some hosts having
   * connectivity problems.
   *
   * exa_admind also puts the currently known leader in the
   * ADMIND_ERR_NOTLEADER response. AdmindClient will add that host to
   * the set if it is not already in it, and will try it in priority.
   *
   * Finally, any errors that occurs while this process is ongoing
   * will be reported to the "warning" callback.
   */
  void send_leader(const AdmindCommand &command,
		   std::set<std::string> &hostnames,
		   MessageFunc inprogress,
		   MessageFunc progressive_payload,
		   MessageFunc done, WarningFunc warning,
		   ErrorFunc error,
		   unsigned int timeout = 0);



private:
  class RequestImpl;
  class Seeker;

  static int connect_node(const std::string &hostname);
  static int get_socket_error(int fd);

  void done_request(Request *request);

  Notifier &notifier;


};


#endif /* __ADMINDCLIENT_H__ */
