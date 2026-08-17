#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>
#include <exception>
#include <algorithm>

#include "boost/asio/awaitable.hpp"
#include "nlohmann/json.hpp"

#include "../domain/Entities.hpp"
#include "../service/MarketService.hpp"

using json = nlohmann::json;

class RequestHandler {
public:
    explicit RequestHandler(std::shared_ptr<MarketService> service);

    boost::asio::awaitable<std::string> HandleRequest(const std::string& request_str);

private:
    std::shared_ptr<MarketService> service_;

    json CreateSuccessResponse();

    json CreateErrorResponse(const std::string& message);

    std::string GetStringParam(const json& j, const std::string& key);

    TimeFrame ParseInterval(const std::string& str);
};