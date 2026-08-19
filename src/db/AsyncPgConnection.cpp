#include "AsyncPgConnection.hpp"

#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

#include "boost/asio/experimental/awaitable_operators.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/use_awaitable.hpp"

namespace asio = boost::asio;
using namespace boost::asio::experimental::awaitable_operators;

namespace {

constexpr auto CONNECT_TIMEOUT = std::chrono::seconds(10);

std::string ErrorText(PGconn* connection) {
    const char* message = (connection == nullptr ? nullptr : PQerrorMessage(connection));
    return message == nullptr ? "unknown PostgreSQL error" : std::string(message);
}

} 

PgResult::PgResult(PGresult* result)
    : result_(result) 
    {}

int PgResult::RowCount() const {
    return result_ == nullptr ? 0 : PQntuples(result_.get());
}

int PgResult::ColumnCount() const {
    return result_ == nullptr ? 0 : PQnfields(result_.get());
}

bool PgResult::IsNull(int row, int column) const {
    if (result_ == nullptr || row < 0 || row >= RowCount() ||
        column < 0 || column >= ColumnCount()) {
        throw std::out_of_range("PostgreSQL result index is out of range");
    }
    return PQgetisnull(result_.get(), row, column) != 0;
}

std::string_view PgResult::Value(int row, int column) const {
    if (IsNull(row, column)) {
        return {};
    }
    return std::string_view(
        PQgetvalue(result_.get(), row, column),
        static_cast<std::size_t>(PQgetlength(result_.get(), row, column)));
}

void PgResult::Deleter::operator()(PGresult* result) const {
    if (result != nullptr) {
        PQclear(result);
    }
}

AsyncPgConnection::AsyncPgConnection(asio::any_io_executor executor, PGconn* connection)
    : executor_(std::move(executor))
    , connection_(connection)
    , socket_(executor_) 
    {}

AsyncPgConnection::~AsyncPgConnection() {
    if (socket_.is_open()) {
        try {
            socket_.release();
        } catch (...) {
        }
    }
    if (connection_ != nullptr) {
        PQfinish(connection_);
    }
}

asio::awaitable<std::shared_ptr<AsyncPgConnection>> AsyncPgConnection::Connect(
        asio::any_io_executor executor,
        const PgConnectOptions& options) {
    const char* keywords[] = {
        "host", "hostaddr", "port", "dbname", "user", "password", nullptr
    };
    const char* values[] = {
        options.host.c_str(),
        options.host_address.c_str(),
        options.port.c_str(),
        options.database.c_str(),
        options.username.c_str(),
        options.password.c_str(),
        nullptr
    };

    PGconn* raw_connection = PQconnectStartParams(keywords, values, 0);
    if (raw_connection == nullptr) {
        throw std::runtime_error("PQconnectStartParams could not allocate a connection");
    }

    auto connection = std::shared_ptr<AsyncPgConnection>(
        new AsyncPgConnection(std::move(executor), raw_connection));

    if (PQstatus(raw_connection) == CONNECTION_BAD) {
        throw connection->Error("PostgreSQL connection start failed");
    }

    const auto deadline = std::chrono::steady_clock::now() + CONNECT_TIMEOUT;
    while (true) {
        const auto status = PQconnectPoll(raw_connection);
        switch (status) {
            case PGRES_POLLING_OK:
                if (PQsetnonblocking(raw_connection, 1) != 0) {
                    throw connection->Error("Could not enable PostgreSQL nonblocking mode");
                }
                co_return connection;

            case PGRES_POLLING_READING:
                connection->RefreshSocket();
                co_await connection->Wait(asio::posix::stream_descriptor::wait_read, deadline);
                break;

            case PGRES_POLLING_WRITING:
                connection->RefreshSocket();
                co_await connection->Wait(asio::posix::stream_descriptor::wait_write, deadline);
                break;

            case PGRES_POLLING_ACTIVE:
                co_await asio::post(connection->executor_, asio::use_awaitable);
                break;

            case PGRES_POLLING_FAILED:
                throw connection->Error("PostgreSQL connection failed");
        }
    }
}

asio::awaitable<PgResult> AsyncPgConnection::Query(std::string sql, PgParams params) {
    if (!IsHealthy()) {
        throw Error("PostgreSQL connection is not healthy");
    }
    if (params.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Too many PostgreSQL query parameters");
    }

    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& param : params) {
        values.push_back(param.has_value() ? param->c_str() : nullptr);
    }

    if (PQsendQueryParams(
            connection_,
            sql.c_str(),
            static_cast<int>(values.size()),
            nullptr,
            values.data(),
            nullptr,
            nullptr,
            0) == 0) {
        throw Error("Could not send PostgreSQL query");
    }

    co_await FlushOutput();

    PgResult last_result;
    std::string query_error;
    bool received_result = false;

    while (true) {
        while (PQisBusy(connection_) != 0) {
            co_await Wait(asio::posix::stream_descriptor::wait_read);
            if (PQconsumeInput(connection_) == 0) {
                throw Error("Could not read PostgreSQL response");
            }
        }

        PGresult* raw_result = PQgetResult(connection_);
        if (raw_result == nullptr) {
            break;
        }

        received_result = true;
        const auto status = PQresultStatus(raw_result);
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK && 
                status != PGRES_EMPTY_QUERY) {
            const char* message = PQresultErrorMessage(raw_result);
            query_error = message == nullptr ? "PostgreSQL query failed" : std::string(message);
        }
        last_result = PgResult(raw_result);
    }

    if (!query_error.empty()) {
        throw std::runtime_error(query_error);
    }
    if (!received_result) {
        throw Error("PostgreSQL query returned no result");
    }

    co_return last_result;
}

bool AsyncPgConnection::IsHealthy() const {
    return connection_ != nullptr && PQstatus(connection_) == CONNECTION_OK;
}

void AsyncPgConnection::RefreshSocket() {
    const int descriptor = PQsocket(connection_);
    if (descriptor < 0) {
        throw Error("PostgreSQL connection has no socket");
    }
    if (socket_.is_open() && socket_.native_handle() == descriptor) {
        return;
    }

    if (socket_.is_open()) {
        socket_.release();
    }
    boost::system::error_code error;
    socket_.assign(descriptor, error);
    if (error) {
        throw boost::system::system_error(error);
    }
}

asio::awaitable<void> AsyncPgConnection::Wait(
        asio::posix::stream_descriptor::wait_type type,
        std::optional<std::chrono::steady_clock::time_point> deadline) {
    RefreshSocket();

    if (!deadline.has_value()) {
        co_await socket_.async_wait(type, asio::use_awaitable);
        co_return;
    }

    asio::steady_timer timer(executor_);
    timer.expires_at(*deadline);
    timer.async_wait([weak = weak_from_this()](const boost::system::error_code& error) {
        if (error) {
            return;
        }
        if (auto self = weak.lock()) {
            boost::system::error_code ignored;
            self->socket_.cancel(ignored);
        }
    });

    boost::system::error_code wait_error;
    co_await socket_.async_wait(
        type, asio::redirect_error(asio::use_awaitable, wait_error));
    boost::system::error_code ignored;
    timer.cancel(ignored);

    if (wait_error == asio::error::operation_aborted &&
        std::chrono::steady_clock::now() >= *deadline) {
        throw std::runtime_error("PostgreSQL connection timed out");
    }
    if (wait_error) {
        throw boost::system::system_error(wait_error);
    }
}

asio::awaitable<void> AsyncPgConnection::WaitReadableOrWritable() {
    RefreshSocket();
    co_await (
        socket_.async_wait(asio::posix::stream_descriptor::wait_read, asio::use_awaitable) ||
        socket_.async_wait(asio::posix::stream_descriptor::wait_write, asio::use_awaitable));
}

asio::awaitable<void> AsyncPgConnection::FlushOutput() {
    while (true) {
        const int status = PQflush(connection_);
        if (status == 0) {
            co_return;
        }
        if (status < 0) {
            throw Error("Could not flush PostgreSQL query");
        }

        co_await WaitReadableOrWritable();
        if (PQconsumeInput(connection_) == 0) {
            throw Error("Could not consume PostgreSQL input while flushing");
        }
    }
}

std::runtime_error AsyncPgConnection::Error(const std::string& context) const {
    return std::runtime_error(context + ": " + ErrorText(connection_));
}