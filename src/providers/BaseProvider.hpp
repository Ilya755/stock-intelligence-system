#pragma once

#include <memory>

#include "nlohmann/json.hpp"

#include "../http/IHttpClient.hpp"
#include "../common/Config.hpp"
#include "../common/RateLimiter.hpp"

using json = nlohmann::json;

class BaseProvider {
public:
    BaseProvider(std::shared_ptr<IHttpClient> client,
                    const std::string& api_key, 
                    const std::string& base_url,
                    const Limits& limits);

    virtual ~BaseProvider() = default;

protected:
    std::shared_ptr<IHttpClient> client_;
    std::string api_key_;
    std::string base_url_;
    std::unique_ptr<RateLimiter> rate_limiter_;

    std::optional<nlohmann::json> PerformGet(const std::string& endpoint, 
                                                std::map<std::string, std::string> params = {});

    double ParseDoubleSafe(const std::string& str);
};