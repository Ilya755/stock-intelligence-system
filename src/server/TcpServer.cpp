#include "TcpServer.hpp"

#include "../common/Logger.hpp"
#include "TcpSession.hpp" 

using boost::asio::ip::tcp;

TcpServer::TcpServer(boost::asio::io_context& io_context, 
                        int port, 
                        std::shared_ptr<RequestHandler> handler,
                        std::shared_ptr<ThreadPool> thread_pool)
    : acceptor_(io_context)
    , handler_(handler)
    , thread_pool_(thread_pool) {
        tcp::endpoint endpoint(tcp::v4(), port);
        
        try {
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();
            
            Logger::Info("[TcpServer] Server is listening on port {}", port);

            DoAccept();
        } catch (const std::exception& ex) {
            Logger::Critical("[TcpServer] Failed to start server on port {}: {}", port, ex.what());
            throw;
        }
    }

void TcpServer::DoAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<TcpSession>(std::move(socket), handler_, thread_pool_)->Start();
            } else {
                Logger::Error("[TcpServer] Accept error: {}", ec.message());
            }

            DoAccept();
        });
}