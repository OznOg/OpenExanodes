/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/include/admindclient.h"

#include <boost/lexical_cast.hpp>
#include <errno.h>
#include <fcntl.h>

#include "os/include/os_network.h"
#include "os/include/os_error.h"

#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/src/admindclient_request.h"
#include "ui/common/src/admindclient_seeker.h"

#include <functional>

using boost::lexical_cast;
using std::bind;
using std::exception;
using std::placeholders::_1;
using std::runtime_error;
using std::set;
using std::string;


AdmindClient::AdmindClient(Notifier &_notifier):
  notifier(_notifier)
{
}


void
AdmindClient::send_node(const AdmindCommand &command, const string &hostname,
                        MessageFunc inprogress, MessageFunc progressive_payload,
                        MessageFunc done, ErrorFunc error, unsigned int timeout)
{
    try
    {
        /*
           TODO : move all the underlying processing outside RequestImpl
           constructor.
           this should not be a memory leak as long as done_request
           is called, and it always should.
         */
        new RequestImpl(*this, command.get_name(), command.get_xml_command(),
                               hostname, connect_node(hostname),
                               inprogress, progressive_payload, done, error,
                               timeout,
                               bind(&AdmindClient::done_request, this, _1));
    }
    catch (exception &ex)
    {
        if (error)
        {
            error(ex.what());
        }
    }

}


void
AdmindClient::send_leader(const AdmindCommand &command,
                          set<string> &hostnames,
                          MessageFunc inprogress,
                          MessageFunc progressive_payload,
                          MessageFunc done,
                          WarningFunc warning,
                          ErrorFunc error,
                          unsigned int timeout)
{
    /*
       TODO : move all the underlying processing outside RequestImpl
       constructor.
       this should not be a memory leak as long as done_request
       is called, and it always should.

     */
    new Seeker(*this, command.get_name(),
               command.get_xml_command(),
               hostnames, inprogress, progressive_payload,
               done, warning,
               error, timeout);
}




int AdmindClient::connect_node(const string &hostname)
{
  struct addrinfo hints;
  struct addrinfo *info;
  struct addrinfo *ptr;
  string errorinfo("getaddrinfo did not return any information");
  int rv;
  int fd(-1);
  const string service(lexical_cast<string>(ADMIND_SOCKET_PORT));
  int optval;
  socklen_t optlen = sizeof(optval);


  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET; /* For now, Exanodes only works on IPv4 */

  /* FIXME: use os_host_addr */
  rv = getaddrinfo(hostname.c_str(), service.c_str(), &hints, &info);
  if (rv != 0)
#ifdef WIN32
    throw runtime_error(gai_strerror(rv));
#else  /* WIN32 */
  throw runtime_error(rv == EAI_SYSTEM ? strerror(errno) : gai_strerror(rv));
#endif  /* WIN32 */

  for (ptr = info; ptr; ptr = ptr->ai_next)
    {
      fd = os_socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
      if ( fd < 0)
	{
	  errorinfo = strerror(-fd);
	  continue;
	}

      optval = 1;
      rv = os_setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,  ( const char *) & optval, optlen);
      if (rv < 0)
	{
	  errorinfo = strerror(-rv);
	  continue;
	}

      do
	{
	  rv = os_connect(fd, ptr->ai_addr, ptr->ai_addrlen);
	}
      while (rv == -EINTR);

      if (rv < 0 && rv != -EINPROGRESS)
	{
	  errorinfo = strerror(errno);
	  os_closesocket(fd);
	  fd = -1;
	}
      else
	break; // don't loop if we have a good connection
    }

  freeaddrinfo(info);

  if (fd == -1)
    throw runtime_error(errorinfo);

  return fd;
}


int AdmindClient::get_socket_error(int fd)
{
  int errorval(0);
  int len(sizeof(errorval));

  errorval = os_getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *) &errorval, &len);

  return -errorval;
}

void
AdmindClient::done_request(Request *request)
{
    delete request;
}
