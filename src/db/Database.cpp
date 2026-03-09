#include "Database.hpp"

#include "../common/Config.hpp"

Database::Database() {
    constexpr int MIN_COUNT_CONN = 4;

    int pool_size = Config::GetInstance().GetServer().max_threads; 
    pool_ = std::make_unique<ConnectionPool>(std::max(pool_size, MIN_COUNT_CONN));
}

std::shared_ptr<ConnectionPool::ConnectionGuard> Database::GetConnection() {
    return pool_->Acquire();
}