#pragma once

#include <memory>
#include <string>

#include "boost/asio.hpp"

#include "RequestHandler.hpp"

using boost::asio::ip::tcp;

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(tcp::socket socket, std::shared_ptr<RequestHandler> handler);

    void Start();

    ~TcpSession();

private:
    tcp::socket socket_;

    boost::asio::streambuf buffer_;
    std::string remote_endpoint_;
    
    std::shared_ptr<RequestHandler> handler_;

    boost::asio::awaitable<void> Run();
};