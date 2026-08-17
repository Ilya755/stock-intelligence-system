#include "FinnhubProvider.hpp"

#include "../../common/Logger.hpp"

FinnhubProvider::FinnhubProvider(std::shared_ptr<IHttpClient> client,
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
                    "[Finnhub] Unknown capability '{}' in config", 
                    s);
            }
        }
    }

std::string FinnhubProvider::GetName() const { 
    return name_; 
}

bool FinnhubProvider::HasCapability(const ProviderCapability cap) const {
    return stock_caps_.contains(cap);
}

boost::asio::awaitable<int> FinnhubProvider::GetRemainingRequests() const { co_return -1; }    // ToDo

boost::asio::awaitable<std::optional<double>> FinnhubProvider::GetStockPrice(const std::string& ticker) {
    if (!HasCapability(ProviderCapability::PriceRealtime)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/quote", {{"symbol", ticker}});

    if (!json_opt.has_value()) {
        co_return std::nullopt;
    }

    try {
        double price = (json_opt.value())["c"].get<double>();

        if (price == 0.0) {
            Logger::Warn(
                "[Finnhub] Price is 0 for ticker '{}'. Symbol might be invalid.", 
                ticker);
            co_return std::nullopt;
        }
        co_return price;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] Price parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<StockPriceCandle>> FinnhubProvider::GetStockHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (interval == TimeFrame::Daily && !HasCapability(ProviderCapability::PriceDaily)) {
        co_return std::vector<StockPriceCandle>{};
    } else if ((interval == TimeFrame::Minute1 || interval == TimeFrame::Minute5 || 
                    interval == TimeFrame::Minute15 || interval == TimeFrame::Hourly) && 
                    !HasCapability(ProviderCapability::PriceIntraday)) {
        co_return std::vector<StockPriceCandle>{};
    } else if (interval == TimeFrame::Weekly || interval == TimeFrame::Monthly && 
                    !HasCapability(ProviderCapability::PriceHistoryDeep)) {
        co_return std::vector<StockPriceCandle>{};
    }

    std::string resolution =  ConvertInterval(interval);

    long long from_ts = std::chrono::duration_cast<std::chrono::seconds>(from.time_since_epoch()).count();
    long long to_ts = std::chrono::duration_cast<std::chrono::seconds>(to.time_since_epoch()).count();

    auto json_opt = co_await PerformGetAsync("/stock/candle", {
                                {"symbol", ticker},
                                {"resolution", resolution},
                                {"from", std::to_string(from_ts)},
                                {"to", std::to_string(to_ts)}
                            });

    if (!json_opt.has_value()) {
        co_return std::vector<StockPriceCandle>{};
    }
    
    std::string status = (json_opt.value()).value("s", "error");
    if (status != "ok") {
        if (status == "no_data") {
            Logger::Info(
                "[Finnhub] No data for ticker '{}'", 
                ticker);
        } else {
            Logger::Warn(
                "[Finnhub] Status '{}' for ticker '{}'", 
                status, ticker);
        }
        co_return std::vector<StockPriceCandle>{};
    }

    std::vector<StockPriceCandle> results;
    try {
        const auto& j = json_opt.value();
        
        if (!j.contains("t") || !j.contains("c") || !j["t"].is_array()) {
            co_return std::vector<StockPriceCandle>{};
        }

        const auto& t = j["t"];
        const auto& o = j["o"];
        const auto& h = j["h"];
        const auto& l = j["l"];
        const auto& c = j["c"];
        const auto& v = j["v"];

        size_t count = t.size();

        results.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            StockPriceCandle candle;
            candle.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(t[i].get<long long>()));
            candle.open = o[i].get<double>();
            candle.high = h[i].get<double>();
            candle.low = l[i].get<double>();
            candle.close = c[i].get<double>();
            candle.volume = v[i].get<long long>();

            results.push_back(candle);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] History parse error for '{}': {}", 
            ticker, ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::optional<CompanyFullInfo>> FinnhubProvider::GetCompanyProfile(
        const std::string& ticker) {
    if (!HasCapability(ProviderCapability::CompanyProfile)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/stock/profile2", {{"symbol", ticker}});
    
    if (!json_opt.has_value() || json_opt->empty()) {
        co_return std::nullopt;
    }

    try {
        const auto& j = json_opt.value();
        
        CompanyFullInfo info;
        info.ticker = j.value("ticker", ticker);
        info.name = j.value("name", "Unknown");
        info.country = j.value("country", "");
        info.currency = j.value("currency", "USD");
        info.exchange = j.value("exchange", "");
        info.sector = j.value("finnhubIndustry", "");
        info.industry = "";
        info.description = "";
        info.updated_at = std::chrono::system_clock::now();
        
        co_return info;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] Profile parse error for '{}': {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<TickerSearchResult>> FinnhubProvider::SearchStockTicker(
        const std::string& ticker) {
    auto json_opt = co_await PerformGetAsync("/search", {{"q", ticker}});

    if (!json_opt.has_value() || !json_opt->contains("result")) {
        co_return std::vector<TickerSearchResult>{};
    }

    std::vector<TickerSearchResult> results;
    try {
        const auto& data = (json_opt.value())["result"];

        if (!data.is_array()) {
            co_return std::vector<TickerSearchResult>{};
        }
        
        results.reserve(data.size());

        for (const auto& item : data) {
            if (item.value("type", "") != "Common Stock") {
                continue;
            }

            TickerSearchResult res;
            res.name = item.value("description", "");
            res.ticker = item.value("symbol", "");
            res.type = "Equity";
            res.region = "";

            results.push_back(res);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] Search parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<StockDividends>> FinnhubProvider::GetDividends(
        const std::string& ticker, 
        const Date from, 
        const Date to) {
    if (!HasCapability(ProviderCapability::Dividends)) {
        co_return std::vector<StockDividends>{};
    }

    auto json_opt = co_await PerformGetAsync("/stock/dividend", {
                                {"symbol", ticker},
                                {"from", TimeUtils::DateToString(from)},
                                {"to", TimeUtils::DateToString(to)}
                            });

    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<StockDividends>{};
    }

    std::vector<StockDividends> result;
    try {
        const auto& data = json_opt.value();
        result.reserve(data.size());

        for (const auto& item : data) {
            StockDividends d;
            d.ex_date = TimeUtils::StringToDate(item.value("date", ""));
            d.amount = item.value("amount", 0.0);
            
            if (item.contains("payDate") && !item["payDate"].is_null()) {
                std::string pay_str = item["payDate"].get<std::string>();
                if (!pay_str.empty()) {
                    d.payment_date = TimeUtils::StringToDate(pay_str);
                }
            }
            result.push_back(d);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] Parse Dividends error for '{}': {}", 
            ticker, ex.what());
    }
    co_return result;
}

boost::asio::awaitable<std::optional<AnalystRating>> FinnhubProvider::GetAnalystRatings(
        const std::string& ticker) {
    if (!HasCapability(ProviderCapability::AnalystRatings)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/stock/recommendation", {{"symbol", ticker}});
    
    if (!json_opt.has_value() || !json_opt->is_array() || json_opt->empty()) {
        co_return std::nullopt;
    }

    try {
        const auto& fresh = (json_opt.value())[0];

        AnalystRating ar;
        ar.date = TimeUtils::StringToDate(fresh.value("period", ""));
        ar.buy = fresh.value("buy", 0);
        ar.hold = fresh.value("hold", 0);
        ar.sell = fresh.value("sell", 0);
        ar.target_price = 0.0; 

        co_return ar;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] Failed to parse Analyst Ratings for '{}': {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<MarketNews>> FinnhubProvider::GetCompanyNews(
        const std::string& ticker, 
        const int limit) {
    if (!HasCapability(ProviderCapability::News)) {
        co_return std::vector<MarketNews>{};
    }

    auto now = std::chrono::system_clock::now();
    auto week_ago = now - std::chrono::hours(24 * 7);
    
    auto json_opt = co_await PerformGetAsync("/company-news", {
                                {"symbol", ticker},
                                {"from", TimeUtils::DateToString(Date(std::chrono::floor<std::chrono::days>(week_ago)))},
                                {"to", TimeUtils::DateToString(Date(std::chrono::floor<std::chrono::days>(now)))}
                            });

    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<MarketNews>{};
    }

    const auto& j = json_opt.value();

    std::vector<MarketNews> news_list;    

    const size_t count = std::min((size_t) limit, j.size());
    
    news_list.reserve(count);

    for (size_t i = 0; i < j.size() && i < count; ++i) {
        try {
            const auto& item = j[i];
            MarketNews n;
            n.datetime = std::chrono::system_clock::time_point(std::chrono::seconds(item.value("datetime", 0LL)));
            n.headline = item.value("headline", "");
            n.source = item.value("source", "");
            n.url = item.value("url", "");
            n.summary = item.value("summary", "");
            n.sentiment_score = 0.0; 

            news_list.push_back(n);
        } catch (const std::exception& ex) {
            Logger::Error(
                "[Finnhub] Failed to parse Company News for '{}': {}", 
                ticker, ex.what());
        }
    }
    co_return news_list;
}

boost::asio::awaitable<std::vector<MarketNews>> FinnhubProvider::GetMarketNews(
        const std::string& category, 
        const int limit) {
    if (!HasCapability(ProviderCapability::News)) {
        co_return std::vector<MarketNews>{};
    }

    auto json_opt = co_await PerformGetAsync("/news", {{"category", category}});
    
    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<MarketNews>{};
    }

    const auto& j = json_opt.value();
    
    std::vector<MarketNews> news_list;    

    const size_t count = std::min((size_t) limit, j.size());

    news_list.reserve(count);
    for (size_t i = 0; i < j.size() && i < count; ++i) {
        try {
            const auto& item = j[i];
            MarketNews n;
            n.datetime = std::chrono::system_clock::time_point(std::chrono::seconds(item.value("datetime", 0LL)));
            n.headline = item.value("headline", "");
            n.source = item.value("source", "");
            n.url = item.value("url", "");
            n.summary = item.value("summary", "");

            news_list.push_back(n);
        } catch (const std::exception& ex) {
            Logger::Error(
                "[Finnhub] Failed to parse Market News for category '{}': {}", 
                category, ex.what());
        }
    }
    co_return news_list;
}

bool FinnhubProvider::HasCapability(const CryptoCapability cap) const {
    return crypto_caps_.contains(cap);
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> FinnhubProvider::GetCryptoHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (!HasCapability(CryptoCapability::History)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    try {
        auto stock_candles = co_await GetStockHistory(ticker, from, to, interval);
        
        std::vector<CryptoPriceCandle> crypto_candles;

        crypto_candles.reserve(stock_candles.size());
        
        for (const auto& sc : stock_candles) {
            CryptoPriceCandle cc;
            cc.timestamp = sc.timestamp;
            cc.open = sc.open;
            cc.high = sc.high;
            cc.low = sc.low;
            cc.close = sc.close;
            cc.volume = static_cast<double>(sc.volume);

            crypto_candles.push_back(cc);
        }
        co_return crypto_candles;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Finnhub] Crypto History error: {}", 
            ex.what());
        co_return std::vector<CryptoPriceCandle>{};
    }
}

boost::asio::awaitable<std::vector<StockSplit>> FinnhubProvider::GetStockSplits(
        const std::string&, 
        const Date, 
        const Date) { 
    if (HasCapability(ProviderCapability::StockSplits)) {
        Logger::Warn(
            "[Finnhub] Splits enabled but not implemented.");
    }
    co_return std::vector<StockSplit>{}; 
}

boost::asio::awaitable<std::vector<CompanyFinancialReport>> FinnhubProvider::GetFinancialReports(
        const std::string&) {
    if (HasCapability(ProviderCapability::FinancialsDeep)) {
        Logger::Warn(
            "[Finnhub] Deep Financials require premium.");
    }
    co_return std::vector<CompanyFinancialReport>{}; 
}

boost::asio::awaitable<std::vector<InsiderTransaction>> FinnhubProvider::GetInsiderTransactions(
        const std::string&, const int) {
    if (HasCapability(ProviderCapability::Insiders)) {
        Logger::Warn(
            "[Finnhub] Insiders enabled but not implemented.");
    }
    co_return std::vector<InsiderTransaction>{};
}

boost::asio::awaitable<std::map<std::string, double>> FinnhubProvider::GetTechnicalIndicator(
        const std::string&, 
        const TechIndicatorType, 
        const TimeFrame) {
    if (HasCapability(ProviderCapability::TechIndicators)) {
        Logger::Warn(
            "[Finnhub] TechIndicators enabled but not implemented.");
    }
    co_return std::map<std::string, double>{};
}

boost::asio::awaitable<std::vector<CalendarEvent>> FinnhubProvider::GetEarningsCalendar(
        const Date, 
        const Date) {
    if (HasCapability(ProviderCapability::Earnings)) {
        Logger::Warn(
            "[Finnhub] Earnings enabled but not implemented.");
    }
    co_return std::vector<CalendarEvent>{};
}

boost::asio::awaitable<std::vector<EconomicIndicator>> FinnhubProvider::GetMacroIndicator(
        const MacroIndicatorType) {
    if (HasCapability(ProviderCapability::MacroEconomics)) {
        Logger::Warn(
            "[Finnhub] Macro enabled but not implemented.");
    }
    co_return std::vector<EconomicIndicator>{};
}

boost::asio::awaitable<std::optional<double>> FinnhubProvider::GetCryptoPrice(const std::string&) { 
    if (HasCapability(CryptoCapability::RealtimePrice)) {
        Logger::Warn(
            "[Finnhub] CryptoPrice enabled but not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::optional<CryptoAsset>> FinnhubProvider::GetCryptoAssetInfo(
        const std::string&) { 
    if (HasCapability(CryptoCapability::Metadata)) {
        Logger::Warn(
            "[Finnhub] CryptoMeta enabled but not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::vector<CryptoAsset>> FinnhubProvider::GetCryptoTopList(const int) { 
    if (HasCapability(CryptoCapability::TopList)) {
        Logger::Warn(
            "[Finnhub] CryptoTopList enabled but not implemented.");
    }
    co_return std::vector<CryptoAsset>{}; 
}

boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> FinnhubProvider::GetGlobalMetrics() { 
    if (HasCapability(CryptoCapability::GlobalMetrics)) {
        Logger::Warn(
            "[Finnhub] CryptoGlobal enabled but not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::optional<OrderBook>> FinnhubProvider::GetOrderBook(
        const std::string&, 
        const int) { 
    if (HasCapability(CryptoCapability::OrderBook)) {
        Logger::Warn(
            "[Finnhub] CryptoBook enabled but not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::vector<CryptoAsset>> FinnhubProvider::SearchAsset(const std::string&) { 
    co_return std::vector<CryptoAsset>{}; 
}

std::string FinnhubProvider::ConvertInterval(const TimeFrame tf)  {
    switch (tf) {
        case TimeFrame::Minute1: 
            return "1";
        case TimeFrame::Minute5: 
            return "5";
        case TimeFrame::Minute15: 
            return "15";
        case TimeFrame::Hourly: 
            return "60";
        case TimeFrame::Daily: 
            return "D";
        case TimeFrame::Weekly: 
            return "W";
        case TimeFrame::Monthly: 
            return "M";
        default: 
            throw std::runtime_error("Interval not supported by Finnhub");
    }
}