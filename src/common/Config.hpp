#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <shared_mutex>

struct Limits {
    int requests_per_day;
    int requests_per_minute;
};

struct ApiConfig {
    std::string name;
    std::string base_url;
    std::string api_key;
    int priority;
    bool enabled;
    std::string description;
    Limits limits;
    std::vector<std::string> capabilities;
};

struct ServerConfig {
    int port;
    int read_timeout_seconds;
};

struct DatabaseConfig {
    std::string type;
    std::string name;
    std::string host;
    int port;
    std::string username;
    std::string password;
    int pool_size = 4;
};


class Config {
public:
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    static Config& GetInstance();

    void Reload();

    ServerConfig GetServer() const;
    DatabaseConfig GetDatabase() const;
    ApiConfig GetProvider(const std::string& key) const;
    std::map<std::string, ApiConfig> GetAllProviders() const;
    std::string GetLoggerPath() const;

private:
    Config();

    void LoadFromFile();
    
    ServerConfig server_;
    DatabaseConfig db_;
    std::map<std::string, ApiConfig> providers_;
    std::string logger_path_;

    mutable std::shared_mutex mtx;
};