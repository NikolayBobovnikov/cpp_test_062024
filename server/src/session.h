#pragma once

#include <array>
#include <boost/asio.hpp>
#include <memory>

class Server;

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, Server& server);
    void start();

private:
    void _read();
    void _processBuffer();
    void _write();

    tcp::socket m_socket;
    Server& m_server;
    static constexpr std::size_t max_length = 1024;
    std::array<char, max_length> m_data;
    std::string m_response;
    std::string m_buffer;
};
