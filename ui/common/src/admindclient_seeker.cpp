/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/src/admindclient_seeker.h"

#include <boost/bind.hpp>

#include "ui/common/include/admindmessage.h"
#include "ui/common/include/notifier.h"
#include "os/include/os_network.h"

using boost::bind;
using boost::shared_ptr;
using std::exception;
using std::map;
using std::set;
using std::string;


AdmindClient::Seeker::Seeker(AdmindClient &_client,
                             const string &_cmd_name,
                             const string &_xml_cmd,
			     set<string> &hostnames,
			     MessageFunc _inprogress,
			     MessageFunc _progressive_payload,
			     MessageFunc _done,
			     WarningFunc _warning, ErrorFunc _error,
			     unsigned int _timeout):
  client(_client),
  command_name(_cmd_name),
  xml_cmd(_xml_cmd),
  inprogress(_inprogress),
  progressive_payload(_progressive_payload),
  done(_done),
  warning(_warning),
  error(_error),
  finished(false)
{
  set<string>::iterator it;

  if (_timeout)
    timeout = client.notifier.get_timer(_timeout, bind(&Seeker::do_timeout,
						       this));

  for (it = hostnames.begin(); it != hostnames.end(); ++it)
    connect(*it);

  maybe_try();
}


AdmindClient::Seeker::~Seeker()
{
  map<string, int>::iterator it;

  for (it = connecting.begin(); it != connecting.end(); ++it)
  {
    client.notifier.del_write(it->second);
    os_closesocket(it->second);
  }

  for (it = connected.begin(); it != connected.end(); ++it)
    os_closesocket(it->second);
}


void AdmindClient::Seeker::handle_connect(const string &hostname, int fd)
{
  int rv;

  connecting.erase(hostname);
  client.notifier.del_write(fd);

  rv = get_socket_error(fd);
  if (rv)
  {
    os_closesocket(fd);
    do_warning(hostname, string("could not connect: ") + strerror(rv));
  }
  else
    connected.insert(make_pair(hostname, fd));

  maybe_try();
}


void AdmindClient::Seeker::handle_done(const AdmindMessage &message,
				       const string &hostname)
{
  err = message.get_error_code();
  switch (message.get_error_code())
  {
  case EXA_ERR_BAD_PROTOCOL:
    do_warning(hostname, "Protocol release does not match.");
    break;

    /* FIXME: Those messages need to be "fine-tuned". It should also
     * be remembered that they are common to the CLI and the GUI, so
     * they should NOT be word-wrapped... */
  case EXA_ERR_ADMIND_NOCONFIG:
    do_warning(hostname, "Exanodes is in a bad state (no configuration)");
    break;

  case EXA_ERR_ADMIND_STOPPED:
    do_warning(hostname, "Exanodes is in a bad state (stopped)");
   break;

  case EXA_ERR_ADMIND_STOPPING:
    do_warning(hostname, "Exanodes is in a bad state (stopping)");
    break;

  case EXA_ERR_ADMIND_STARTING:
    do_warning(hostname, "Exanodes is in a bad state (starting)");
    break;

  case EXA_ERR_ADMIND_STARTED:
    do_warning(hostname, "Exanodes is in a bad state (started)");
    break;

  case ADMIND_ERR_NOTLEADER:
    /* This node was not the leader, this is not an error, we just try the next
     * one in our list. */
    break;

  default:
    do_done(message);
  }
}


void AdmindClient::Seeker::handle_error(const string &info,
					const string &hostname)
{
  terminate();
  do_warning(hostname, info);
}


void AdmindClient::Seeker::connect(const string &hostname)
{
  try {
    int fd = connect_node(hostname);
    connecting.insert(make_pair(hostname, fd));
    client.notifier.add_write(fd,
			      bind(&AdmindClient::Seeker::handle_connect,
				   this, hostname, fd));
  }
  catch (exception &ex)
  {
    do_warning(hostname, string("could not connect: ") + ex.what());
  }
}


void AdmindClient::Seeker::do_done(const AdmindMessage &message)
{
  terminate();

  if (done)
    done(message);
}


void AdmindClient::Seeker::do_error(const string &info)
{
  terminate();

  if (error)
    error(info);
}


void AdmindClient::Seeker::do_timeout()
{
  do_error("timed out");
}


void AdmindClient::Seeker::do_warning(const string &hostname,
				      const string &info)
{
  if (warning)
    warning(hostname, info, err);
}


void AdmindClient::Seeker::done_request()
{
  request.reset();
  if (!finished)
    maybe_try();
}


void AdmindClient::Seeker::maybe_try()
{
  if (request)
    return;

  if (connecting.empty() && connected.empty())
    return do_error("could not find a node that could accept the command");

  map<string, int>::iterator it(connected.begin());

  if (it != connected.end())
  {
    const string hostname(it->first);
    int fd(it->second);

    connected.erase(it);
    request = shared_ptr<RequestImpl>(
	      new RequestImpl(client, command_name, xml_cmd, hostname, fd,
			      inprogress,
			      progressive_payload,
			      bind(&AdmindClient::Seeker::handle_done, this, _1,
				   hostname),
			      bind(&AdmindClient::Seeker::handle_error, this, _1,
				   hostname), 0,
			      bind(&AdmindClient::Seeker::done_request, this)));
  }

}


void AdmindClient::Seeker::terminate()
{
  finished = true;
  client.notifier.delay_call(bind(&AdmindClient::done_request, &client,
				  this));
}


