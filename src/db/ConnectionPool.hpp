#pragma once

#include <cstddef>
#include <memory>

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/awaitable.hpp"
#include "boost/asio/experimental/concurrent_channel.hpp"

#include "AsyncPgConnection.hpp"

class ConnectionPool {
public:
    class ConnectionGuard {
    public:
        ConnectionGuard() = default;
        ConnectionGuard(std::shared_ptr<AsyncPgConnection> conn, ConnectionPool& pool);

        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;

        ConnectionGuard(ConnectionGuard&& other) noexcept;
        ConnectionGuard& operator=(ConnectionGuard&& other) noexcept;

        ~ConnectionGuard();

        AsyncPgConnection* operator->() const;
        
        AsyncPgConnection& Get() const;

    private:
        std::shared_ptr<AsyncPgConnection> connection_;
        ConnectionPool* pool_ = nullptr;

        void Reset();
    };


    ConnectionPool(
        boost::asio::any_io_executor executor, 
        std::size_t pool_size,
        PgConnectOptions options);

    ~ConnectionPool();

    boost::asio::awaitable<void> Initialize();

    boost::asio::awaitable<ConnectionGuard> Acquire();

private:
    boost::asio::any_io_executor executor_;
    std::size_t pool_size_;
    PgConnectOptions options_;

    using Channel = boost::asio::experimental::concurrent_channel<
            void(boost::system::error_code, std::shared_ptr<AsyncPgConnection>)>;
    Channel available_;


    void Release(std::shared_ptr<AsyncPgConnection> connection) noexcept;

    boost::asio::awaitable<void> ReplaceConnection();
};