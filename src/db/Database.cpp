#include "Database.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/use_awaitable.hpp"

#include "../common/Config.hpp"

Database::Database(boost::asio::any_io_executor executor)
    : executor_(std::move(executor)) 
    {}

boost::asio::awaitable<void> Database::Initialize() {
    const auto& config = Config::GetInstance().GetDatabase();
    const std::string port = std::to_string(config.port);

    boost::asio::ip::tcp::resolver resolver(executor_);
    const auto endpoints = co_await resolver.async_resolve(
        config.host, port, boost::asio::use_awaitable);
    if (endpoints.empty()) {
        throw std::runtime_error("Could not resolve PostgreSQL host: " + config.host);
    }

    PgConnectOptions options{
        .host = config.host,
        .host_address = endpoints.begin()->endpoint().address().to_string(),
        .port = port,
        .database = config.name,
        .username = config.username,
        .password = config.password
    };

    const auto pool_size = static_cast<std::size_t>(std::max(config.pool_size, 1));
    pool_ = std::make_unique<ConnectionPool>(executor_, pool_size, std::move(options));
    co_await pool_->Initialize();
}

boost::asio::awaitable<PgResult> Database::Query(std::string sql, PgParams params) {
    if (pool_ == nullptr) {
        throw std::logic_error("Database has not been initialized");
    }

    auto connection = co_await pool_->Acquire();
    co_return co_await connection->Query(std::move(sql), std::move(params));
}