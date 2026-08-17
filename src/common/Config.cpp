#include "Config.hpp"

#include "nlohmann/json.hpp"

#include "Logger.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    Limits, requests_per_day, requests_per_minute
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    ApiConfig, base_url, api_key, priority, 
    enabled, description, limits, capabilities
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    ServerConfig, port, read_timeout_seconds, max_threads
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    DatabaseConfig, type, name, host, port, username, password, pool_size
)

Config& Config::GetInstance() {
    static Config config_instance;
    return config_instance;
}

Config::Config() {
    Logger::Debug(
        "[Config] Loading Config...");
    LoadFromFile();
}

void Config::Reload() {
    Logger::Info(
        "[Config] Reloading Config...");
    LoadFromFile();
}

void Config::LoadFromFile() {
    fs::path config_path;
    const char* env_path = std::getenv("APP_CONFIG_PATH");

    if (env_path) {
        config_path = env_path;
        Logger::Debug(
            "[Config] Using config from ENV: {}", config_path.string());
    } else {
        config_path = "/../../configs/app_config.json"; 
        Logger::Debug(
            "[Config] ENV missing or invalid. Trying local path: {}", 
            config_path.string());
    }
    
    if (!fs::exists(config_path)) {
        Logger::Error(
            "[Config] Config file not found at: {}", 
            config_path.string());
        throw std::runtime_error("Missing config file");
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        Logger::Error(
            "[Config] Failed to open file {}", 
            config_path.string());
        throw std::runtime_error("File openning error");
    }

    try {
        json j = json::parse(file);

        if (!j.contains("server")) {
            Logger::Critical(
                "[Config] Missing critical 'server' section");
            throw std::runtime_error("Config missing 'server' section");
        }
        ServerConfig temp_server = j.at("server").get<ServerConfig>();

        if (!j.contains("database")) {
            Logger::Critical(
                "[Config] Missing critical 'database' section");
            throw std::runtime_error("Config missing 'database' section");
        }
        DatabaseConfig temp_db = j.at("database").get<DatabaseConfig>();

        std::map<std::string, ApiConfig> temp_providers;
        if (!j.contains("api_providers")) {
            Logger::Warn(
                "[Config] Section 'api_providers' is missing");
        } else {
            temp_providers = j.at("api_providers").get<std::map<std::string, ApiConfig>>();
            for (auto& [key, val] : providers_) {
                val.name = key;
            }
        }

        std::string temp_logger_path;
        if (j.contains("logger")) {
            if (j["logger"].contains("logs_path")) {
                temp_logger_path = j["logger"]["logs_path"].get<std::string>();
            } else {
                logger_path_ = "logs"; 
                Logger::Warn(
                    "[Config] 'logger.logs_path' missing, using default path: {}", 
                    logger_path_);
            }
        } else {
            temp_logger_path = "logs";
            Logger::Info(
                "[Config] 'logger' section missing, using default path: {}", 
                logger_path_);
        }

        {
            std::unique_lock lock(mtx);
            server_ = std::move(temp_server);
            db_ = std::move(temp_db);
            providers_ = std::move(temp_providers);
            logger_path_ = std::move(temp_logger_path);
        }

        Logger::Info(
            "[Config] Config loaded successfully. Found {} providers.",
            providers_.size());
    } catch (const json::exception& ex) {
        Logger::Error(
            "[Config] JSON parsing failed: {}. Keeping old configuration.", 
            ex.what());
        throw; 
    }
}

ServerConfig Config::GetServer() const { 
    std::shared_lock lock(mtx);
    return server_; 
}

DatabaseConfig Config::GetDatabase() const { 
    std::shared_lock lock(mtx);
    return db_; 
}

ApiConfig Config::GetProvider(const std::string& key) const {
    std::shared_lock lock(mtx);
    try {
        return providers_.at(key);
    } catch (const std::out_of_range&) {
        Logger::Critical(
            "[Config] Requested unknown provider config: '{}'", 
            key);
        throw std::runtime_error("Provider config not found: " + key);
    }
}

std::map<std::string, ApiConfig> Config::GetAllProviders() const {
    std::shared_lock lock(mtx);
    return providers_;
}

std::string Config::GetLoggerPath() const {
    std::shared_lock lock(mtx);
    return logger_path_;
}