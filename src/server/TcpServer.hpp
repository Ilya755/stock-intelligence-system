#pragma once

#include <memory>
#include <string>

#include "boost/asio.hpp"

#include "RequestHandler.hpp"

using boost::asio::ip::tcp;

class TcpServer {
public:
    TcpServer(
        boost::asio::io_context& io_context, 
        int port, 
        std::shared_ptr<RequestHandler> handler);

private:
    tcp::acceptor acceptor_;
    std::shared_ptr<RequestHandler> handler_;

    void DoAccept();
};