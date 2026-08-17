#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/awaitable.hpp"
#include "boost/asio/posix/stream_descriptor.hpp"
#include "libpq-fe.h"

using PgParam = std::optional<std::string>;
using PgParams = std::vector<PgParam>;

struct PgConnectOptions {
    std::string host;
    std::string host_address;
    std::string port;
    std::string database;
    std::string username;
    std::string password;
};

class PgResult {
public:
    PgResult() = default;
    explicit PgResult(PGresult* result);

    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;

    PgResult(PgResult&&) noexcept = default;
    PgResult& operator=(PgResult&&) noexcept = default;

    int RowCount() const;

    int ColumnCount() const;

    bool IsNull(int row, int column) const;

    std::string_view Value(int row, int column) const;

private:
    struct Deleter {
        void operator()(PGresult* result) const;
    };

    std::unique_ptr<PGresult, Deleter> result_;
};

class AsyncPgConnection : public std::enable_shared_from_this<AsyncPgConnection> {
public:
    static boost::asio::awaitable<std::shared_ptr<AsyncPgConnection>> Connect(
        boost::asio::any_io_executor executor,
        const PgConnectOptions& options);

    AsyncPgConnection(const AsyncPgConnection&) = delete;
    AsyncPgConnection& operator=(const AsyncPgConnection&) = delete;

    ~AsyncPgConnection();

    boost::asio::awaitable<PgResult> Query(std::string sql, PgParams params = {});

    bool IsHealthy() const;

private:
    boost::asio::any_io_executor executor_;
    PGconn* connection_;
    boost::asio::posix::stream_descriptor socket_;
    
    AsyncPgConnection(boost::asio::any_io_executor executor, PGconn* connection);

    void RefreshSocket();

    boost::asio::awaitable<void> Wait(
        boost::asio::posix::stream_descriptor::wait_type type,
        std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);

    boost::asio::awaitable<void> WaitReadableOrWritable();

    boost::asio::awaitable<void> FlushOutput();

    std::runtime_error Error(const std::string& context) const;
};