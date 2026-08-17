#include "RequestHandler.hpp"

#include "../common/Logger.hpp"
#include "../common/TimeUtils.hpp"

using json = nlohmann::json;

RequestHandler::RequestHandler(std::shared_ptr<MarketService> service) 
    : service_(service) 
    {}

boost::asio::awaitable<std::string> RequestHandler::HandleRequest(const std::string& request_str) {
    json response;
    std::string command;

    try {
        auto req = json::parse(request_str);
        command = req.value("command", "UNKNOWN");
        
        Logger::Debug(
            "[RequestHandler] Processing command: {}", 
            command);
        
        if (command == "GET_STOCK_PRICE") {
            std::string ticker = GetStringParam(req, "ticker");

            auto price = co_await service_->GetStockPrice(ticker);
            
            if (price.has_value()) {
                response = CreateSuccessResponse();
                response["data"] = json::object({{"ticker", ticker}, 
                                                {"price", price.value()}});
            } else {
                response = CreateErrorResponse("Price not found or external API error");
            }
        } else if (command == "GET_COMPANY_PROFILE") {
            std::string ticker = GetStringParam(req, "ticker");

            auto profile = co_await service_->GetCompanyProfile(ticker);
            
            if (profile.has_value()) {
                response = CreateSuccessResponse();
                response["data"] = json::object({{"id", profile->id},
                                                {"ticker", profile->ticker},
                                                {"name", profile->name},
                                                {"sector", profile->sector.value_or("")},
                                                {"industry", profile->industry.value_or("")},
                                                {"country", profile->country.value_or("")},
                                                {"currency", profile->currency},
                                                {"description", profile->description.value_or("")},
                                                {"exchange", profile->exchange}});
            } else {
                response = CreateErrorResponse("Company Profile not found");
            }
        } else if (command == "GET_STOCK_HISTORY") {
            std::string ticker = GetStringParam(req, "ticker");

            Timestamp from = TimeUtils::StringToTimestamp(GetStringParam(req, "from"));
            Timestamp to = TimeUtils::StringToTimestamp(GetStringParam(req, "to"));
            TimeFrame interval = ParseInterval(GetStringParam(req, "interval"));

            auto history = co_await service_->GetStockHistory(ticker, from, to, interval);
            
            if (history.empty()) {
                response = CreateErrorResponse("No history data found or parameters unsupported by providers.");
            } else {
                json candles_json = json::array();
                for (const auto& candle : history) {
                    candles_json.push_back(json::object({{"timestamp", TimeUtils::TimestampToString(candle.timestamp)},
                                                        {"open", candle.open},
                                                        {"high", candle.high},
                                                        {"low", candle.low},
                                                        {"close", candle.close},
                                                        {"volume", candle.volume}}));
                }
                
                response = CreateSuccessResponse();
                response["data"] = json::object({{"ticker", ticker},
                                                {"count", history.size()},
                                                {"candles", candles_json}});
            }
        } else if (command == "GET_DIVIDENDS") {
            std::string ticker = GetStringParam(req, "ticker");

            Date from = TimeUtils::StringToDate(GetStringParam(req, "from"));
            Date to = TimeUtils::StringToDate(GetStringParam(req, "to"));

            auto divs = co_await service_->GetDividends(ticker, from, to);

            if (divs.empty()) {
                response = CreateErrorResponse("No dividend data found or API key/limit errors.");
            } else {
                json divs_json = json::array();
                for (const auto& d : divs) {
                    divs_json.push_back(json::object({{"ex_date", TimeUtils::DateToString(d.ex_date)},
                                                    {"payment_date", d.payment_date.has_value() ? 
                                                                        TimeUtils::DateToString(d.payment_date.value()) : ""},
                                                    {"amount", d.amount}}));
                }

                response = CreateSuccessResponse();
                response["data"] = divs_json;
            }
        } else if (command == "GET_FINANCIALS_REPORTS") {
            std::string ticker = GetStringParam(req, "ticker");

            auto reports = co_await service_->GetFinancialReports(ticker);

            if (reports.empty()) {
                response = CreateErrorResponse("No financial reports found or API key/limit errors.");
            } else {
                json reports_json = json::array();
                for (const auto& r : reports) {
                    reports_json.push_back(json::object({{"period_date", TimeUtils::DateToString(r.period_date)},
                                                        {"report_type", (r.report_type == ReportType::Annual ? 
                                                                                        "Annual" : "Quarterly")},
                                                        {"revenue", r.revenue},
                                                        {"net_income", r.net_income},
                                                        {"currency", r.currency}}));
                }

                response = CreateSuccessResponse();
                response["data"] = reports_json;
            }
        } else if (command == "SEARCH_TICKER") {
            std::string query = GetStringParam(req, "query");

            auto results = co_await service_->SearchTicker(query);

            if (results.empty()) {
                response = CreateErrorResponse("No ticker found or API key/limit errors.");
            } else {
                json res_json = json::array();
                for (const auto& item : results) {
                    res_json.push_back(json::object({{"ticker", item.ticker},
                                                    {"name", item.name},
                                                    {"type", item.type},
                                                    {"region", item.region}}));
                }

                response = CreateSuccessResponse();
                response["data"] = res_json;
            }
        } else if (command == "GET_CRYPTO_PRICE") {
            std::string ticker = GetStringParam(req, "ticker");

            auto price = co_await service_->GetCryptoPrice(ticker);
            
            if (price.has_value()) {
                response = CreateSuccessResponse();
                response["data"] = json::object({{"ticker", ticker}, 
                                                {"price", price.value()}});
            } else {
                response = CreateErrorResponse("Crypto Price not found");
            }
        } else if (command == "GET_CRYPTO_INFO") {
            std::string ticker = GetStringParam(req, "ticker");

            auto info = co_await service_->GetCryptoAssetInfo(ticker);
            
            if (info.has_value()) {
                response = CreateSuccessResponse();
                response["data"] = json::object({{"ticker", info->ticker},
                                                {"name", info->name}});
            } else {
                response = CreateErrorResponse("Crypto Asset not found");
            }
        } else if (command == "GET_CRYPTO_HISTORY") {
            std::string ticker = GetStringParam(req, "ticker");

            Timestamp from = TimeUtils::StringToTimestamp(GetStringParam(req, "from"));
            Timestamp to = TimeUtils::StringToTimestamp(GetStringParam(req, "to"));
            TimeFrame interval = ParseInterval(GetStringParam(req, "interval"));

            auto history = co_await service_->GetCryptoHistory(ticker, from, to, interval);

            if (history.empty()) {
                response = CreateErrorResponse("No history data found or parameters unsupported by providers.");
            } else {            
                json candles_json = json::array();
                for (const auto& candle : history) {
                    candles_json.push_back(json::object({{"timestamp", TimeUtils::TimestampToString(candle.timestamp)},
                                                        {"open", candle.open},
                                                        {"high", candle.high},
                                                        {"low", candle.low},
                                                        {"close", candle.close},
                                                        {"volume", candle.volume}}));
                }
                
                response = CreateSuccessResponse();
                response["data"] = json::object({{"ticker", ticker},
                                                {"count", history.size()},
                                                {"candles", candles_json}});
            }
        } else if (command == "GET_CRYPTO_TOP") {
            int limit = req.value("limit", 50);

            auto top = co_await service_->GetTopCryptoAssets(limit);

            if (top.empty()) {
                response = CreateErrorResponse("No data found or parameters unsupported by providers.");
            } else {
                json top_json = json::array();
                for (const auto& item : top) {
                    top_json.push_back(json::object({{"ticker", item.ticker},
                                                    {"name", item.name}}));
                }
                response = CreateSuccessResponse();
                response["data"] = top_json;
            }
        } else {
            Logger::Warn(
                "[RequestHandler] Unknown command received: {}", 
                command);
            response = CreateErrorResponse("Unknown command: " + command);
        }
    } catch (const json::exception& ex) {
        Logger::Error(
            "[RequestHandler] JSON Parsing Error: {}", 
            ex.what());
        response = CreateErrorResponse(std::string("Invalid JSON format: ") + ex.what());
    } catch (const std::exception& ex) {
        Logger::Error(
            "[RequestHandler] Processing Error: {}", 
            ex.what());
        response = CreateErrorResponse(std::string("Internal server error: ") + ex.what());
    }

    co_return response.dump() + "\n";
}

json RequestHandler::CreateSuccessResponse() {
    return json::object({{"status", "ok"}});
}

json RequestHandler::CreateErrorResponse(const std::string& message) {
    return json::object({{"status", "error"},
                        {"message", message}});
}

std::string RequestHandler::GetStringParam(const json& j, const std::string& key) {
    if (!j.contains(key)) {
        throw std::runtime_error("Missing required parameter: " + key);
    }
    return j[key].get<std::string>();
}

TimeFrame RequestHandler::ParseInterval(const std::string& str) {
    if (str == "1M") {
        return TimeFrame::Monthly;
    }

    std::string lower_str;
    lower_str.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter(lower_str),
                                                [&](unsigned char c) { return std::tolower(c); });

    static const std::map<std::string, TimeFrame> map = {
        {"1m", TimeFrame::Minute1},
        {"m1", TimeFrame::Minute1}, 
        {"1min", TimeFrame::Minute1},
        {"5m", TimeFrame::Minute5}, 
        {"m5", TimeFrame::Minute5}, 
        {"5min", TimeFrame::Minute5},
        {"15m", TimeFrame::Minute15}, 
        {"m15", TimeFrame::Minute15}, 
        {"15min", TimeFrame::Minute15},

        {"1h", TimeFrame::Hourly}, 
        {"h1", TimeFrame::Hourly}, 
        {"2h", TimeFrame::Hour2}, 
        {"h2", TimeFrame::Hour2},
        {"6h", TimeFrame::Hour6}, 
        {"h6", TimeFrame::Hour6},
        {"12h", TimeFrame::Hour12}, 
        {"h12", TimeFrame::Hour12},

        {"1d", TimeFrame::Daily},
        {"d1", TimeFrame::Daily},

        {"1w", TimeFrame::Weekly},
        {"w1", TimeFrame::Weekly}
    };

    auto it = map.find(lower_str);
    if (it != map.end()) {
        return it->second;
    }
    
    throw std::runtime_error("Invalid interval: " + str);
}