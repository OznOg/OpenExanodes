/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/src/admindclient_request.h"

#include <errno.h>

#include "ui/common/include/admindmessage.h"
#include "os/include/os_network.h"
#include <cassert>
#include <cstring>
#include <functional>

#include <cstring>

using std::shared_ptr;
using std::exception;
using std::string;


#define BOUNDARY "</Admind>"
#define WHITESPACE_CHARS " \t\n\r"


AdmindClient::RequestImpl::RequestImpl(AdmindClient &_client,
                                       const string &cmd_name,
                                       const string &xml_cmd,
				       const string &_connected_node, int _fd,
				       MessageFunc _inprogress,
				       MessageFunc _progressive_payload,
				       MessageFunc _done, ErrorFunc _error,
				       unsigned int _timeout,
				       std::function<void(RequestImpl*)> _cleanup):
  client(_client),
  connected_node(_connected_node),
  command_name(cmd_name),
  fd(_fd),
  inprogress(_inprogress),
  progressive_payload(_progressive_payload),
  done(_done),
  error(_error),
  cleanup(_cleanup)
{
  assert(fd != -1);

  client.notifier.add_write(fd, std::bind(&RequestImpl::state_connecting, this,
				     xml_cmd));

  if (_timeout)
    timeout = client.notifier.get_timer(_timeout,
					std::bind(&RequestImpl::do_timeout, this));
}


AdmindClient::RequestImpl::~RequestImpl()
{
  cleanup_fd();
}


void AdmindClient::RequestImpl::state_connecting(string xml_cmd)
{
  client.notifier.del_write(fd);

  int rv(get_socket_error(fd));
  if (rv)
    return do_error(strerror(-rv));

  client.notifier.add_write(fd, std::bind(&RequestImpl::state_sending, this,
				     xml_cmd));
}


void AdmindClient::RequestImpl::state_sending(string xml_cmd)
{
  int rv;

  do
    rv = os_send(fd, xml_cmd.c_str(), xml_cmd.length());
  while (rv == -EINTR);

  if (rv < 0 && rv != -EAGAIN)
  {
    do_error(string("write: ") + strerror(-rv));
    return;
  }

  if (rv > 0)
    xml_cmd.erase(0, rv);

  /* If there is more to write, we stay in this state. */
  if (xml_cmd.length() > 0)
    client.notifier.add_write(fd,
			      std::bind(&RequestImpl::state_sending, this,
				   xml_cmd));
  else
  {
    client.notifier.del_write(fd);
    client.notifier.add_read(fd,
			     std::bind(&RequestImpl::state_receiving, this, ""));
  }
}


void AdmindClient::RequestImpl::state_receiving(string incoming)
{
  int rv;
  char buf[32768];
  string::size_type index;
  string::size_type start;

  do
    rv = os_recv(fd, buf, sizeof(buf), 0);
  while (rv == -EINTR);

  if (rv < 0 && rv != -EAGAIN)
  {
    do_error(string("read: ") + strerror(-rv));
    return;
  }

  if (rv == 0)
  {
    do_error("Connection lost");
    return;
  }

  if (incoming.length() >= strlen(BOUNDARY))
    start = incoming.length() - strlen(BOUNDARY);
  else
    start = 0;
  incoming.append(buf, rv);

  /* We check for fd != -1, because the terminate() method (which can
   * be called from parse_message) sets it to that value. */
  while (fd != -1 && (index = incoming.find(BOUNDARY, start)) != string::npos)
  {
    string::size_type first;
    /* Retrieve the position of the first non blank character */
    first = incoming.find_first_not_of(WHITESPACE_CHARS);
    /* Remove blank characters from the beginning of the input */
    incoming.erase(0, first);
    /* Decrement index from the number of blank characters removed */
    index -= first;
    parse_message(incoming.substr(0, index + strlen(BOUNDARY)));
    incoming.erase(0, index + strlen(BOUNDARY));
    start = 0;
  }

  client.notifier.add_read(fd,
			   std::bind(&RequestImpl::state_receiving, this,
				incoming));
}


void AdmindClient::RequestImpl::parse_message(const string &xml_reply)
{
  shared_ptr<AdmindMessage> message;

  try
  {
    message = shared_ptr<AdmindMessage>(
	new AdmindMessage(command_name, connected_node, xml_reply));
  }
  catch (exception &ex)
  {
    return do_error(ex.what());
  }

  switch (message->get_type())
  {
      case AdmindMessage::INPROGRESS:
	  do_inprogress(*message);
	  break;
      case AdmindMessage::PAYLOAD:
	  do_progressive_payload(*message);
	  break;
      case AdmindMessage::DONE:
	  do_done(*message);
	  break;
  }
}


void AdmindClient::RequestImpl::do_inprogress(const AdmindMessage &message)
{
  if (inprogress)
    inprogress(message);
}


void AdmindClient::RequestImpl::do_progressive_payload(const AdmindMessage &message)
{
  if (progressive_payload)
    progressive_payload(message);
}


void AdmindClient::RequestImpl::do_done(const AdmindMessage &message)
{
  terminate();

  if (done)
    done(message);
}


void AdmindClient::RequestImpl::do_error(const string &info)
{
  terminate();

  if (error)
    error(info);
}


void AdmindClient::RequestImpl::do_timeout()
{
  do_error("timed out");
}


void AdmindClient::RequestImpl::cleanup_fd()
{
  if (fd != -1)
  {
    client.notifier.del_read(fd);
    client.notifier.del_write(fd);
    os_closesocket(fd);
    os_shutdown(fd, SHUT_RDWR);
    fd = -1;
  }
}


void AdmindClient::RequestImpl::terminate()
{
  cleanup_fd();
  client.notifier.delay_call(std::bind(cleanup, this));
}


