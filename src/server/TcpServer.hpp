#pragma once

#include <memory>
#include <string>

#include "boost/asio.hpp"

#include "../common/ThreadPool.hpp"
#include "RequestHandler.hpp"

using boost::asio::ip::tcp;

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_context, 
                int port, 
                std::shared_ptr<RequestHandler> handler,
                std::shared_ptr<ThreadPool> thread_pool);

private:
    tcp::acceptor acceptor_;
    std::shared_ptr<RequestHandler> handler_;
    std::shared_ptr<ThreadPool> thread_pool_;

    void DoAccept();
};