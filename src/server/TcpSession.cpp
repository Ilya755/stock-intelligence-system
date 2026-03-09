#include "TcpSession.hpp"

#include "../common/Logger.hpp"

using boost::asio::ip::tcp;

TcpSession::TcpSession(tcp::socket socket, 
                        std::shared_ptr<RequestHandler> handler,
                        std::shared_ptr<ThreadPool> thread_pool)
    : socket_(std::move(socket))
    , strand_(socket_.get_executor())
    , handler_(handler)
    , thread_pool_(thread_pool) {
        boost::system::error_code ec;
        socket_.set_option(boost::asio::socket_base::keep_alive(true), ec);
    }

void TcpSession::Start() {
    try {
        remote_endpoint_ = socket_.remote_endpoint().address().to_string();
        Logger::Debug("[TcpSession] Client connected: {}", remote_endpoint_);
    } catch (...) {
        remote_endpoint_ = "Unknown";
    }
    
    DoRead();
}

TcpSession::~TcpSession() {
    Logger::Debug("[TcpSession] Session disconnected: {}", remote_endpoint_);
}

void TcpSession::DoRead() {
    auto self(shared_from_this());
    
    boost::asio::async_read_until(socket_, buffer_, '\n',
        boost::asio::bind_executor(strand_, 
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string request_str;
                    std::getline(is, request_str);

                    if (!request_str.empty()) {
                        thread_pool_->PushTask([this, self, request_str]() {
                            ProcessRequest(request_str);
                        });
                    }
                    
                    DoRead(); 
                } else {
                    if (ec != boost::asio::error::eof) {
                        Logger::Warn("[TcpSession] Read error from {}: {}", 
                            remote_endpoint_, ec.message());
                    }
                }
            }
        ));
}

void TcpSession::ProcessRequest(std::string request) {
    std::string response = handler_->HandleRequest(request);
    Deliver(response);
}

void TcpSession::Deliver(const std::string& msg) {
    auto self(shared_from_this());

    boost::asio::post(strand_, [this, self, msg]() {
        bool write_in_progress = !write_queue_.empty();
        write_queue_.push_back(msg);
        
        if (!write_in_progress) {
            DoWrite();
        }
    });
}

void TcpSession::DoWrite() {
    auto self(shared_from_this());

    boost::asio::async_write(socket_, boost::asio::buffer(write_queue_.front()),
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    write_queue_.pop_front();
                    if (!write_queue_.empty()) {
                        DoWrite();
                    }
                } else {
                    Logger::Error("[TcpSession] Write error to {}: {}", remote_endpoint_, ec.message());
                    socket_.close();
                }
            }
        )
    );
}