#include "TcpSession.hpp"

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/use_awaitable.hpp"

#include "../common/Logger.hpp"

using boost::asio::ip::tcp;

TcpSession::TcpSession(tcp::socket socket, std::shared_ptr<RequestHandler> handler)
    : socket_(std::move(socket))
    , handler_(std::move(handler)) {
        boost::system::error_code ec;
        socket_.set_option(boost::asio::socket_base::keep_alive(true), ec);
    }

void TcpSession::Start() {
    try {
        remote_endpoint_ = socket_.remote_endpoint().address().to_string();
        Logger::Debug(
            "[TcpSession] Client connected: {}", 
            remote_endpoint_);
    } catch (const std::exception&) {
        remote_endpoint_ = "Unknown";
    }

    auto self = shared_from_this();
    boost::asio::co_spawn(
        socket_.get_executor(),
        [self]() -> boost::asio::awaitable<void> {
            co_await self->Run();
        },
        [self](std::exception_ptr error) {
            if (!error) {
                return;
            }
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& ex) {
                Logger::Warn(
                    "[TcpSession] Session error for {}: {}",
                    self->remote_endpoint_, ex.what());
            }
        });
}

TcpSession::~TcpSession() {
    Logger::Debug(
        "[TcpSession] Session disconnected: {}",
        remote_endpoint_);
}

boost::asio::awaitable<void> TcpSession::Run() {
    try {
        while (socket_.is_open()) {
            co_await boost::asio::async_read_until(
                socket_, buffer_, '\n', boost::asio::use_awaitable);

            std::istream input(&buffer_);
            std::string request;
            std::getline(input, request);
            if (request.empty()) {
                continue;
            }

            const std::string response = co_await handler_->HandleRequest(request);
            co_await boost::asio::async_write(
                socket_, boost::asio::buffer(response), boost::asio::use_awaitable);
        }
    } catch (const boost::system::system_error& ex) {
        if (ex.code() != boost::asio::error::eof &&
            ex.code() != boost::asio::error::operation_aborted) {
            throw;
        }
    }
}