#pragma once

#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <format>

#include "pqxx/pqxx"

class ConnectionPool {
public:
    class ConnectionGuard {
    public:
        ConnectionGuard(std::shared_ptr<pqxx::connection> conn, ConnectionPool& pool);

        ~ConnectionGuard();

        pqxx::connection& Get();

    private:
        std::shared_ptr<pqxx::connection> conn_;
        ConnectionPool& pool_;
    };


    ConnectionPool(size_t pool_size);

    std::shared_ptr<ConnectionGuard> Acquire();

private:
    std::queue<std::shared_ptr<pqxx::connection>> connections_;
    std::mutex mutex_;
    std::condition_variable cv_;

    void ReturnConnection(std::shared_ptr<pqxx::connection> conn);
};