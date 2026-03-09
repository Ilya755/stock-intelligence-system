#include "ConnectionPool.hpp"

#include "../common/Logger.hpp"
#include "../common/Config.hpp"

ConnectionPool::ConnectionGuard::ConnectionGuard(std::shared_ptr<pqxx::connection> conn, ConnectionPool& pool)
    : conn_(conn)
    , pool_(pool) 
    {}

ConnectionPool::ConnectionGuard::~ConnectionGuard() {
    pool_.ReturnConnection(conn_);
}

pqxx::connection& ConnectionPool::ConnectionGuard::Get() { 
    return *conn_; 
}

ConnectionPool::ConnectionPool(size_t pool_size) {
    try {
        const auto& db_cfg = Config::GetInstance().GetDatabase();
        
        std::string conn_str = std::format(
            "host={} port={} dbname={} user={} password={}",
            db_cfg.host, db_cfg.port, db_cfg.name, db_cfg.username, db_cfg.password
        );

        for (size_t i = 0; i < pool_size; ++i) {
            connections_.push(std::make_shared<pqxx::connection>(conn_str));
        }

        Logger::Info("[ConnectionPool] ConnectionPool initialized with {} connections", pool_size);
    } catch (const std::exception& ex) {
        Logger::Critical("[ConnectionPool] ConnectionPool initialization failed: {}", ex.what());
        throw;
    }
}

std::shared_ptr<ConnectionPool::ConnectionGuard> ConnectionPool::Acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    cv_.wait(lock, [this] { return !connections_.empty(); });

    auto conn = connections_.front();
    connections_.pop();
    
    return std::make_shared<ConnectionPool::ConnectionGuard>(conn, *this);
}

void ConnectionPool::ReturnConnection(std::shared_ptr<pqxx::connection> conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(conn);
    }

    cv_.notify_one();
}