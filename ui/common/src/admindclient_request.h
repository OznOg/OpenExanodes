/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADMINDCLIENT_REQUEST_H__
#define __ADMINDCLIENT_REQUEST_H__

#include "ui/common/include/admindclient.h"
#include "ui/common/include/notifier.h"


class AdmindClient::Request : private boost::noncopyable
{
public:

    virtual ~Request() { }

};


class AdmindClient::RequestImpl : public Request
{
public:

    RequestImpl(AdmindClient & _client,
                const std::string & cmd_name,
                const std::string & xml_cmd,
                const std::string & connected_node,
                int _fd,
                MessageFunc _inprogress,
                MessageFunc _progressive_payload,
                MessageFunc _done,
                ErrorFunc _error,
                unsigned int _timeout,
                boost::function < void (RequestImpl *) > _cleanup);
    ~RequestImpl();

private:

    void state_connecting(std::string xml_cmd);
    void state_sending(std::string xml_cmd);
    void state_receiving(std::string incoming);

    void parse_message(const std::string &xml_reply);

    void do_inprogress(const AdmindMessage &message);
    void do_progressive_payload(const AdmindMessage &message);
    void do_done(const AdmindMessage &message);
    void do_error(const std::string &info);
    void do_timeout();

    void cleanup_fd();
    void terminate();

    AdmindClient &client;
    std::string connected_node;
    std::string command_name;
    int fd;
    MessageFunc inprogress;
    MessageFunc progressive_payload;
    MessageFunc done;
    ErrorFunc error;
    boost::function < void (RequestImpl *) > cleanup;
    boost::shared_ptr<Notifier::Timer>timeout;
};


#endif /* __ADMINDCLIENT_REQUEST_H__ */
