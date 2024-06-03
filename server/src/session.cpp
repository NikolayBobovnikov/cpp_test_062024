#include "session.h"
#include "server.h"

Session::Session(tcp::socket socket, Server& server)
    : m_socket(std::move(socket))
    , m_server(server)
{ }

void Session::start()
{
    _read();
}


void Session::_read()
{
    auto self(shared_from_this());
    m_socket.async_read_some(boost::asio::buffer(m_data),
                             [this, self](boost::system::error_code ec, std::size_t length) {
                                 if(!ec)
                                 {
                                     std::string_view command(m_data.data(), length);
                                     m_server.handle_request(command, m_response);
                                     _write();
                                 }
                                 else if(ec == boost::asio::error::eof)
                                 {
                                     std::cout << "Client disconnected.\n";
                                 }
                                 else if(ec == boost::asio::error::connection_reset)
                                 {
                                     std::cout << "Client disconnected forcibly.\n";
                                 }
                                 else
                                 {
                                     std::cerr << "Error on receive: " << ec.message() << "\n";
                                 }
                             });
}

void Session::_write()
{
    auto self(shared_from_this());
    auto handleWriteResult = [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if(!ec)
        {
            _read();
        }
        else
        {
            std::cerr << "Error on send: " << ec.message() << "\n";
        }
    };

    boost::asio::async_write(m_socket, boost::asio::buffer(m_response), handleWriteResult);
}
