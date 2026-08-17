#pragma once

#include <memory>
#include <string>

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/awaitable.hpp"

#include "AsyncPgConnection.hpp"
#include "ConnectionPool.hpp"

class Database {
public:
    explicit Database(boost::asio::any_io_executor executor);

    boost::asio::awaitable<void> Initialize();
    
    boost::asio::awaitable<PgResult> Query(std::string sql, PgParams params = {});

private:
    boost::asio::any_io_executor executor_;
    std::unique_ptr<ConnectionPool> pool_;
};