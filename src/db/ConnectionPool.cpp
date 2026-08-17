#include "ConnectionPool.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/use_awaitable.hpp"

#include "../common/Logger.hpp"

ConnectionPool::ConnectionGuard::ConnectionGuard(
        std::shared_ptr<AsyncPgConnection> connection, 
        ConnectionPool& pool)
    : connection_(std::move(connection))
    , pool_(&pool) 
    {}

ConnectionPool::ConnectionGuard::ConnectionGuard(ConnectionGuard&& other) noexcept
    : connection_(std::move(other.connection_))
    , pool_(std::exchange(other.pool_, nullptr)) 
    {}

ConnectionPool::ConnectionGuard& ConnectionPool::ConnectionGuard::operator=(
        ConnectionGuard&& other) noexcept {
    if (this != &other) {
        Reset();
        connection_ = std::move(other.connection_);
        pool_ = std::exchange(other.pool_, nullptr);
    }
    return *this;
}

ConnectionPool::ConnectionGuard::~ConnectionGuard() {
    Reset();
}

AsyncPgConnection* ConnectionPool::ConnectionGuard::operator->() const {
    return connection_.get();
}

AsyncPgConnection& ConnectionPool::ConnectionGuard::Get() const { 
    if (connection_ == nullptr) {
        throw std::logic_error("PostgreSQL connection guard is empty");
    }
    return *connection_; 
}

void ConnectionPool::ConnectionGuard::Reset() {
    if (pool_ != nullptr && connection_ != nullptr) {
        pool_->Release(std::move(connection_));
    }
    pool_ = nullptr;
}

ConnectionPool::ConnectionPool(
        boost::asio::any_io_executor executor, 
        std::size_t pool_size,
        PgConnectOptions options)
    : executor_(std::move(executor))
    , pool_size_(pool_size)
    , options_(std::move(options))
    , available_(executor_, pool_size_) 
    {}

ConnectionPool::~ConnectionPool() {
    available_.close();
}

boost::asio::awaitable<void> ConnectionPool::Initialize() {
    for (std::size_t i = 0; i < pool_size_; ++i) {
        auto connection = co_await AsyncPgConnection::Connect(executor_, options_);
        if (!available_.try_send(boost::system::error_code{}, std::move(connection))) {
            throw std::runtime_error("Could not add PostgreSQL connection to pool");
        }
    }
    Logger::Info(
        "[ConnectionPool] Async connection pool initialized with {} connections",
        pool_size_);
}

boost::asio::awaitable<ConnectionPool::ConnectionGuard> ConnectionPool::Acquire() {
    auto connection = co_await available_.async_receive(boost::asio::use_awaitable);
    co_return ConnectionGuard(std::move(connection), *this);
}

void ConnectionPool::Release(std::shared_ptr<AsyncPgConnection> connection) noexcept {
    if (connection == nullptr) {
        return;
    }
    if (!connection->IsHealthy()) {
        Logger::Error(
            "[ConnectionPool] Replacing an unhealthy PostgreSQL connection");
        boost::asio::co_spawn(executor_, ReplaceConnection(), boost::asio::detached);
        return;
    }
    if (!available_.try_send(
            boost::system::error_code{}, std::move(connection)) && available_.is_open()) {
        Logger::Critical(
            "[ConnectionPool] Could not return PostgreSQL connection to pool");
    }
}

boost::asio::awaitable<void> ConnectionPool::ReplaceConnection() {
    while (available_.is_open()) {
        try {
            auto connection = co_await AsyncPgConnection::Connect(executor_, options_);
            if (available_.try_send(
                    boost::system::error_code{}, std::move(connection))) {
                Logger::Info(
                    "[ConnectionPool] PostgreSQL connection restored");
            }
            co_return;
        } catch (const std::exception& ex) {
            Logger::Error(
                "[ConnectionPool] PostgreSQL reconnect failed: {}. Retrying...",
                ex.what());
        }

        boost::asio::steady_timer retry_timer(executor_, std::chrono::seconds(1));
        co_await retry_timer.async_wait(boost::asio::use_awaitable);
    }
}