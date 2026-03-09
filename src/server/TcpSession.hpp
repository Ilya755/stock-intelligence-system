#pragma once

#include <memory>
#include <string>
#include <deque>
#include <thread>
#include <iostream>

#include "boost/asio.hpp"

#include "../common/ThreadPool.hpp"
#include "RequestHandler.hpp"

using boost::asio::ip::tcp;

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(tcp::socket socket, 
               std::shared_ptr<RequestHandler> handler,
               std::shared_ptr<ThreadPool> thread_pool);

    void Start();

    ~TcpSession();

private:
    tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;

    boost::asio::streambuf buffer_;
    std::string remote_endpoint_;
    std::deque<std::string> write_queue_; 
    
    std::shared_ptr<RequestHandler> handler_;
    std::shared_ptr<ThreadPool> thread_pool_;

    void DoRead();

    void ProcessRequest(std::string request);

    void Deliver(const std::string& msg);

    void DoWrite();
};