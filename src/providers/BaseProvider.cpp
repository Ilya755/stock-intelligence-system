#include "BaseProvider.hpp"

#include "../common/Logger.hpp"
#include "ProviderCapabilities.hpp"

BaseProvider::BaseProvider(std::shared_ptr<IHttpClient> client,
                            const std::string& api_key, 
                            const std::string& base_url,
                            const Limits& limits)
    : client_(client)
    , api_key_(api_key)
    , base_url_(base_url) 
    , rate_limiter_(std::make_unique<RateLimiter>(limits))
    {}

boost::asio::awaitable<std::optional<nlohmann::json>> BaseProvider::PerformGetAsync(
        const std::string& endpoint, 
        std::map<std::string, std::string> params) {
    if (!rate_limiter_->TryAcquire()) {
        Logger::Warn(
            "[BaseProvider] Request to {} blocked by RateLimiter", 
            base_url_);
        co_return std::nullopt;
    }

    const std::string full_url = base_url_ + endpoint;
    
    if (!api_key_.empty()) {
        params["apikey"] = api_key_; 
    }

    const HttpResponse response = co_await client_->GetAsync(full_url, params);

    if (response.status_code != 200) {
        Logger::Error(
            "[BaseProvider] API error {} returned code {} with error message {}", 
            full_url, response.status_code, response.error_message);
        co_return std::nullopt;
    }

    try {
        co_return nlohmann::json::parse(response.text);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[BaseProvider] JSON parsing error {}", 
            ex.what());
        co_return std::nullopt;
    }
}

double BaseProvider::ParseDoubleSafe(const std::string& str) {
    if (str.empty() || str == "None" || str == "null" || str == "NaN") {
        return 0.0;
    }
    try {
        return std::stod(str);
    } catch (...) {
        Logger::Warn("Failed to parse double from string: '{}'", str);
        return 0.0;
    }
}