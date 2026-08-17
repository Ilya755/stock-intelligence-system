#include "TwelveDataProvider.hpp"

#include "../../common/Logger.hpp"

TwelveDataProvider::TwelveDataProvider(std::shared_ptr<IHttpClient> client, 
                                        const std::string& name,
                                        const std::string& api_key,
                                        const std::string& base_url,
                                        const Limits& limits,
                                        const std::vector<std::string>& config_caps) 
    : BaseProvider(client, api_key, base_url, limits)
    , name_(name) {
        for (const auto& s : config_caps) {
            auto stock_cap = StringToProviderCapability(s);
            auto crypto_cap = StringToCryptoCapability(s);

            if (stock_cap.has_value()) {
                stock_caps_.insert(stock_cap.value());
            } else if (crypto_cap.has_value()) {
                crypto_caps_.insert(crypto_cap.value());
            } else {
                Logger::Warn(
                    "[TwelveData] Unknown capability '{}' in config", 
                    s);
            }
        }
    }

std::string TwelveDataProvider::GetName() const { 
    return name_; 
}

bool TwelveDataProvider::HasCapability(const ProviderCapability cap) const {
    return stock_caps_.contains(cap);
}

bool TwelveDataProvider::HasCapability(const CryptoCapability cap) const {
    return crypto_caps_.contains(cap);
}

boost::asio::awaitable<int> TwelveDataProvider::GetRemainingRequests() const { co_return -1; } // ToDo

boost::asio::awaitable<std::optional<double>> TwelveDataProvider::GetStockPrice(const std::string& ticker) {
    if (!HasCapability(ProviderCapability::PriceRealtime) && 
            !HasCapability(ProviderCapability::PriceIntraday)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/price", {{"symbol", ticker}});
    
    if (!json_opt.has_value() || !json_opt->contains("price")) {
        co_return std::nullopt;
    }

    double price = ParseDoubleSafe((json_opt.value())["price"].get<std::string>());
    if (price == 0.0) {
        co_return std::nullopt;
    }
    co_return price;

}

boost::asio::awaitable<std::vector<StockPriceCandle>> TwelveDataProvider::GetStockHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (!HasCapability(ProviderCapability::PriceIntraday) && 
        !HasCapability(ProviderCapability::PriceDaily)) {
        co_return std::vector<StockPriceCandle>{};
    }

    co_return co_await FetchHistory<StockPriceCandle>(ticker, interval, 
                                            [this](const nlohmann::json& j) { return ParseStockCandle(j); });
}

boost::asio::awaitable<std::optional<CompanyFullInfo>> TwelveDataProvider::GetCompanyProfile(
        const std::string& ticker) {
    if (!HasCapability(ProviderCapability::CompanyProfile)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/profile", {{"symbol", ticker}});

    if (!json_opt.has_value() || !json_opt->contains("symbol")) {
        co_return std::nullopt;
    }

    try {
        const auto& j = json_opt.value();

        CompanyFullInfo info;
        info.ticker = j["symbol"];
        info.name = j["name"];
        info.exchange = j.value("exchange", "");
        info.currency = j.value("currency", "USD");
        info.country = j.value("country", "");
        info.sector = j.value("sector", "");
        info.industry = j.value("industry", "");
        info.description = j.value("description", "");
        info.updated_at = std::chrono::system_clock::now();

        co_return info;
    } catch (const std::exception& ex) { 
        Logger::Error(
            "[TwelveData] Profile parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt; 
    }
}

boost::asio::awaitable<std::vector<StockDividends>> TwelveDataProvider::GetDividends(
        const std::string& ticker, 
        const Date from, 
        const Date to) {
    if (!HasCapability(ProviderCapability::Dividends)) {
        co_return std::vector<StockDividends>{};
    }

    auto json_opt = co_await PerformGetAsync("/dividends", {
                                {"symbol", ticker},
                                {"start_date", TimeUtils::DateToString(from)},
                                {"end_date", TimeUtils::DateToString(to)}
                            });

    if (!json_opt.has_value() || !json_opt->contains("dividends")) {
        co_return std::vector<StockDividends>{};
    }

    std::vector<StockDividends> result;
    try {
        for (const auto& item : (json_opt.value())["dividends"]) {
            StockDividends div;
            div.ex_date = TimeUtils::StringToDate(item["ex_date"].get<std::string>());
            div.amount = item["amount"].get<double>();
            
            if (item.contains("pay_date") && !item["pay_date"].is_null()) {
                div.payment_date = TimeUtils::StringToDate(item["pay_date"].get<std::string>());
            }

            result.push_back(div);
        }
    } catch (const std::exception& ex) { 
        Logger::Error(
            "[TwelveData] Failed to parse Dividends for {}: {}", 
            ticker, ex.what());
    }
    co_return result;
}

boost::asio::awaitable<std::vector<TickerSearchResult>> TwelveDataProvider::SearchStockTicker(
        const std::string& ticker) {
    auto json_opt = co_await PerformGetAsync("/symbol_search", {{"symbol", ticker}});

    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::vector<TickerSearchResult>{};
    }
    
    std::vector<TickerSearchResult> results;
    try {
        const auto& data = (json_opt.value())["data"];
        
        if (!data.is_array()) {
            co_return std::vector<TickerSearchResult>{};
        }

        results.reserve(data.size());

        for (const auto& item : data) {
            if (item.value("instrument_type", "") != "Common Stock") {
                continue;
            }

            TickerSearchResult res;
            res.name = item.value("instrument_name", "");
            res.ticker = item.value("symbol", "");
            res.type = "Equity";
            res.region = item.value("country", "");

            results.push_back(res);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[TwelveData] Search parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::optional<double>> TwelveDataProvider::GetCryptoPrice(
        const std::string& ticker) {
    if (!HasCapability(CryptoCapability::RealtimePrice)) {
        co_return std::nullopt;
    }
    
    co_return co_await GetStockPrice(ticker); 
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> TwelveDataProvider::GetCryptoHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (!HasCapability(CryptoCapability::History)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    co_return co_await FetchHistory<CryptoPriceCandle>(ticker, interval, 
                                                [this](const nlohmann::json& j) { return ParseCryptoCandle(j); });
}

boost::asio::awaitable<std::optional<CryptoAsset>> TwelveDataProvider::GetCryptoAssetInfo(
        const std::string& ticker) {
    if (!HasCapability(CryptoCapability::Metadata)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/profile", {{"symbol", ticker}});

    if (!json_opt.has_value() || !json_opt->contains("symbol")) {
        co_return std::nullopt;
    }

    try {
        CryptoAsset asset;
        asset.id = -1;
        asset.ticker = (json_opt.value())["symbol"];
        asset.name = (json_opt.value()).value("name", asset.ticker); 

        co_return asset;
    } catch (const std::exception& ex) { 
        Logger::Error(
            "[TwelveData] Crypto Profile parse error for {}: {}",
            ticker, ex.what());
        co_return std::nullopt; 
    }
}

boost::asio::awaitable<std::vector<CryptoAsset>> TwelveDataProvider::SearchAsset(
        const std::string& query) {
    auto json_opt = co_await PerformGetAsync("/symbol_search", {{"symbol", query}});

    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::vector<CryptoAsset>{};
    }
    
    std::vector<CryptoAsset> results;
    try {
        const auto& data = (json_opt.value())["data"];
        
        if (!data.is_array()) {
            co_return std::vector<CryptoAsset>{};
        }

        results.reserve(data.size());

        for(const auto& item : data) {
            std::string type = item.value("instrument_type", "");
            if (type != "Cryptocurrency" && type != "Digital Currency") {
                continue;
            }

            CryptoAsset asset;
            asset.ticker = item["symbol"];
            asset.name = item["instrument_name"];

            results.push_back(asset);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[TwelveData] Crypto Search parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<CompanyFinancialReport>> TwelveDataProvider::GetFinancialReports(
        const std::string&) { 
    if (HasCapability(ProviderCapability::FinancialsDeep)) {
        Logger::Warn(
            "[TwelveData] Financials not implemented.");
    }
    co_return std::vector<CompanyFinancialReport>{}; 
}

boost::asio::awaitable<std::vector<InsiderTransaction>> TwelveDataProvider::GetInsiderTransactions(
        const std::string&, int) { 
    if (HasCapability(ProviderCapability::Insiders)) {
        Logger::Warn(
            "[TwelveData] Insiders not implemented.");
    }
    co_return std::vector<InsiderTransaction>{}; 
}

boost::asio::awaitable<std::optional<AnalystRating>> TwelveDataProvider::GetAnalystRatings(
        const std::string&) { 
    if (HasCapability(ProviderCapability::AnalystRatings)) {
        Logger::Warn(
            "[TwelveData] Ratings not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::map<std::string, double>> TwelveDataProvider::GetTechnicalIndicator(
        const std::string&, 
        const TechIndicatorType, 
        const TimeFrame) { 
    if (HasCapability(ProviderCapability::TechIndicators)) {
        Logger::Warn(
            "[TwelveData] TechIndicators not implemented.");
    }
    co_return std::map<std::string, double>{}; 
}

boost::asio::awaitable<std::vector<MarketNews>> TwelveDataProvider::GetCompanyNews(
        const std::string&, 
        const int) { 
    if (HasCapability(ProviderCapability::News)) {
        Logger::Warn(
            "[TwelveData] CompanyNews not implemented.");
    }
    co_return std::vector<MarketNews>{}; 
}

boost::asio::awaitable<std::vector<MarketNews>> TwelveDataProvider::GetMarketNews(
        const std::string&, 
        const int) { 
    if (HasCapability(ProviderCapability::News)) {
        Logger::Warn(
            "[TwelveData] MarketNews not implemented.");
    }
    co_return std::vector<MarketNews>{}; 
}

boost::asio::awaitable<std::vector<CalendarEvent>> TwelveDataProvider::GetEarningsCalendar(
        const Date, 
        const Date) { 
    if (HasCapability(ProviderCapability::Earnings)) {
        Logger::Warn(
            "[TwelveData] Earnings not implemented.");
    }
    co_return std::vector<CalendarEvent>{}; 
}

boost::asio::awaitable<std::vector<EconomicIndicator>> TwelveDataProvider::GetMacroIndicator(
        const MacroIndicatorType) { 
    if (HasCapability(ProviderCapability::MacroEconomics)) {
        Logger::Warn(
            "[TwelveData] MacroIndicator not implemented.");
    }
    co_return std::vector<EconomicIndicator>{}; 
}

boost::asio::awaitable<std::vector<StockSplit>> TwelveDataProvider::GetStockSplits(
        const std::string&, 
        const Date, 
        const Date) {
    if (HasCapability(ProviderCapability::StockSplits)) {
        Logger::Warn(
            "[TwelveData] Stock Splits not implemented.");
    }
    co_return std::vector<StockSplit>{};
}

boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> TwelveDataProvider::GetGlobalMetrics() { 
    if (HasCapability(CryptoCapability::GlobalMetrics)) {
        Logger::Warn(
            "[TwelveData] CryptoGlobal not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::vector<CryptoAsset>> TwelveDataProvider::GetCryptoTopList(int) { 
    if (HasCapability(CryptoCapability::TopList)) {
        Logger::Warn(
            "[TwelveData] CryptoTopList not implemented.");
    }
    co_return std::vector<CryptoAsset>{}; 
}

boost::asio::awaitable<std::optional<OrderBook>> TwelveDataProvider::GetOrderBook(
        const std::string&, 
        const int) { 
    if (HasCapability(CryptoCapability::OrderBook)) {
        Logger::Warn(
            "[TwelveData] CryptoBook not implemented.");
    }
    co_return std::nullopt; 
}

std::string TwelveDataProvider::ConvertInterval(const TimeFrame tf) {
    switch (tf) {
        case TimeFrame::Minute1: 
            return "1min";
        case TimeFrame::Minute5: 
            return "5min";
        case TimeFrame::Minute15: 
            return "15min";
        case TimeFrame::Hourly: 
            return "1h";
        case TimeFrame::Daily: 
            return "1day";
        case TimeFrame::Weekly: 
            return "1week";
        case TimeFrame::Monthly: 
            return "1month";
        default: 
            throw std::runtime_error("Interval not supported by TwelveData");
    }
}

template <typename T>
boost::asio::awaitable<std::vector<T>> TwelveDataProvider::FetchHistory(
        const std::string& symbol, 
        const TimeFrame interval,
        std::function<T(const nlohmann::json&)> parser) {
    std::map<std::string, std::string> params = {
        {"symbol", symbol},
        {"interval", ConvertInterval(interval)},
        {"outputsize", "300"}, 
        {"order", "DESC"}      
    };

    auto json_opt = co_await PerformGetAsync("/time_series", params);
    
    if (!json_opt.has_value() || !json_opt->contains("values")) {
        if (json_opt.has_value() && json_opt->contains("status") && (json_opt.value())["status"] == "error") {
            Logger::Warn(
                "TwelveData Error for {}: {}", 
                symbol, (json_opt.value())["message"].template get<std::string>());
        }
        co_return std::vector<T>{};
    }

    std::vector<T> results;
    try {
        const auto& values = (json_opt.value())["values"];
        
        if (!values.is_array()) {
            co_return std::vector<T>{};
        }

        results.reserve(values.size());

        for (const auto& item : values) {
            results.push_back(parser(item));
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[TwelveData] Parsing history failed for {}: {}", 
            symbol, ex.what());
    }

    std::reverse(results.begin(), results.end());
    co_return results;
}

StockPriceCandle TwelveDataProvider::ParseStockCandle(const nlohmann::json& item) {
    StockPriceCandle c;
    c.timestamp = TimeUtils::StringToTimestamp(item["datetime"].get<std::string>());
    c.open = ParseDoubleSafe(item["open"].get<std::string>());
    c.high = ParseDoubleSafe(item["high"].get<std::string>());
    c.low = ParseDoubleSafe(item["low"].get<std::string>());
    c.close = ParseDoubleSafe(item["close"].get<std::string>());
    c.volume = std::stoll(item["volume"].get<std::string>());
    
    return c;
}

CryptoPriceCandle TwelveDataProvider::ParseCryptoCandle(const nlohmann::json& item) {
    CryptoPriceCandle c;
    c.timestamp = TimeUtils::StringToTimestamp(item["datetime"].get<std::string>());
    c.open = ParseDoubleSafe(item["open"].get<std::string>());
    c.high = ParseDoubleSafe(item["high"].get<std::string>());
    c.low = ParseDoubleSafe(item["low"].get<std::string>());
    c.close = ParseDoubleSafe(item["close"].get<std::string>());
    c.volume = ParseDoubleSafe(item["volume"].get<std::string>());

    return c;
}