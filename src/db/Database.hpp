#pragma once

#include <memory>
#include <algorithm>

#include "ConnectionPool.hpp"

class Database {
public:
    Database();

    std::shared_ptr<ConnectionPool::ConnectionGuard> GetConnection();

private:
    std::unique_ptr<ConnectionPool> pool_;
};