#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "server.h"

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, Server& server)
        : socket_(std::move(socket))
        , server_(server)
    { }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
                                    if(!ec)
                                    {
                                        server_.handle_request(data_, length, response_);
                                        do_write();
                                    }
                                });
    }

    void do_write()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(response_),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                     if(!ec)
                                     {
                                         do_read();
                                     }
                                 });
    }

    tcp::socket socket_;
    Server& server_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    std::string response_;
};
