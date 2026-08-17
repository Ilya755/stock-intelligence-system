#include "AlphaVantageProvider.hpp"

#include "../../common/Logger.hpp"

AlphaVantageProvider::AlphaVantageProvider(
        std::shared_ptr<IHttpClient> client, 
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
                    "[AlphaVantage] Unknown capability '{}' in config", 
                    s);
            }
        }
    }

std::string AlphaVantageProvider::GetName() const { 
    return name_; 
}

bool AlphaVantageProvider::HasCapability(const ProviderCapability cap) const {
    return stock_caps_.contains(cap);
}

boost::asio::awaitable<int> AlphaVantageProvider::GetRemainingRequests() const { co_return -1; } // ToDo: do correct realization

boost::asio::awaitable<std::optional<double>> AlphaVantageProvider::GetStockPrice(
        const std::string& ticker) {
    if (!HasCapability(ProviderCapability::PriceRealtime) && 
            !HasCapability(ProviderCapability::PriceIntraday)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformAVRequestAsync({
                            {"function", "GLOBAL_QUOTE"},
                            {"symbol", ticker}
                        });

    if (!json_opt.has_value() || !json_opt->contains("Global Quote")) {
        co_return std::nullopt;
    }

    try {
        const auto& q = (json_opt.value())["Global Quote"];
        std::string price_str = q.value("05. price", "");
        if (price_str.empty()) {
            co_return std::nullopt;
        }

        co_return std::stod(price_str);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Quote parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<StockPriceCandle>> AlphaVantageProvider::GetStockHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    std::string function_name;
    std::string time_key;

    if (interval == TimeFrame::Daily) {
        if (!HasCapability(ProviderCapability::PriceDaily)) {
            co_return std::vector<StockPriceCandle>{};
        }
        function_name = "TIME_SERIES_DAILY";
        time_key = "Daily Time Series";
    } else if (interval == TimeFrame::Weekly) {
        if (!HasCapability(ProviderCapability::PriceHistoryDeep)) {
            co_return std::vector<StockPriceCandle>{};
        }
        function_name = "TIME_SERIES_WEEKLY";
        time_key = "Weekly Time Series";
    } else if (interval == TimeFrame::Monthly) {
        if (!HasCapability(ProviderCapability::PriceHistoryDeep)) {
            co_return std::vector<StockPriceCandle>{};
        }
        function_name = "TIME_SERIES_MONTHLY";
        time_key = "Monthly Time Series";
    } else {
        if (!HasCapability(ProviderCapability::PriceIntraday)) {
            co_return std::vector<StockPriceCandle>{};
        }
        function_name = "TIME_SERIES_INTRADAY";
        time_key = "Time Series (" + ConvertInterval(interval) + ")"; 
    }

    std::map<std::string, std::string> params = {{"function", function_name},
                                                    {"symbol", ticker},
                                                    {"outputsize", "full"}};

    if (interval != TimeFrame::Daily && interval != TimeFrame::Weekly && interval != TimeFrame::Monthly) {
        params["interval"] = ConvertInterval(interval);
    }

    auto json_opt = co_await PerformAVRequestAsync(params);

    if (!json_opt.has_value()) {
        co_return std::vector<StockPriceCandle>{};
    }

    if (!json_opt->contains(time_key)) {
        Logger::Warn(
            "[AlphaVantage] History response missing key '{}' for {}.", 
            time_key, ticker);
        co_return std::vector<StockPriceCandle>{};
    }

    std::vector<StockPriceCandle> results;
    try {
        const auto& series = (json_opt.value())[time_key];

        if (!series.is_array()){
            co_return std::vector<StockPriceCandle>{};
        }
        
        results.reserve(series.size()); 
        
        for (auto it = series.begin(); it != series.end(); ++it) {
            Timestamp ts = TimeUtils::StringToTimestamp(it.key());

            if (ts < from || ts > to) {
                continue;  
            }

            const auto& val = it.value();
            StockPriceCandle c;
            c.timestamp = ts;
            c.open = ParseDoubleSafe(val.value("1. open", "0"));
            c.high = ParseDoubleSafe(val.value("2. high", "0"));
            c.low = ParseDoubleSafe(val.value("3. low", "0"));
            c.close = ParseDoubleSafe(val.value("4. close", "0"));
            c.volume = std::stoll(val.value("5. volume", "0"));

            results.push_back(c);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] History parse error for {}: {}", 
            ticker, ex.what());
    }

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.timestamp < b.timestamp;
    });

    co_return results;
}

boost::asio::awaitable<std::optional<CompanyFullInfo>> AlphaVantageProvider::GetCompanyProfile(
        const std::string& ticker) {
    if (!HasCapability(ProviderCapability::CompanyProfile)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformAVRequestAsync({{"function", "OVERVIEW"}, {"symbol", ticker}});

    if (!json_opt.has_value() || json_opt->empty()) {
        co_return std::nullopt;
    }

    try {
        const auto& j = json_opt.value();

        CompanyFullInfo info;
        info.ticker = j.value("Symbol", ticker);
        info.name = j.value("Name", "");
        info.description = j.value("Description", "");
        info.exchange = j.value("Exchange", "");
        info.currency = j.value("Currency", "USD");
        info.country = j.value("Country", "");
        info.sector = j.value("Sector", "");
        info.industry = j.value("Industry", "");
        info.updated_at = std::chrono::system_clock::now();

        co_return info;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Profile parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CompanyFinancialReport>> AlphaVantageProvider::GetFinancialReports(
        const std::string& ticker) {
    if (!HasCapability(ProviderCapability::FinancialsDeep)) {
        co_return std::vector<CompanyFinancialReport>{};
    }

    auto json_opt = co_await PerformAVRequestAsync({{"function", "INCOME_STATEMENT"}, {"symbol", ticker}});
    
    if (!json_opt.has_value() || !json_opt->contains("annualReports")) {
        co_return std::vector<CompanyFinancialReport>{};
    }

    std::vector<CompanyFinancialReport> reports;
    try {
        const auto& data = (json_opt.value())["annualReports"];

        if (!data.is_array()) {
            co_return std::vector<CompanyFinancialReport>{};
        }

        reports.reserve(data.size());

        for (const auto& item : data) {
            CompanyFinancialReport r;
            r.period_date = TimeUtils::StringToDate(item.value("fiscalDateEnding", ""));
            r.currency = item.value("reportedCurrency", "USD");
            r.report_type = ReportType::Annual;
            r.revenue = ParseDoubleSafe(item.value("totalRevenue", "0"));
            r.net_income = ParseDoubleSafe(item.value("netIncome", "0"));
            r.total_debt = 0.0; 
            r.equity = 0.0;

            reports.push_back(r);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Financials parse error for {}: {}", 
            ticker, ex.what());
    }
    co_return reports;
}

boost::asio::awaitable<std::vector<EconomicIndicator>> AlphaVantageProvider::GetMacroIndicator(
        const MacroIndicatorType type) {
    if (!HasCapability(ProviderCapability::MacroEconomics)) {
        co_return std::vector<EconomicIndicator>{};
    }

    std::string function;
    switch (type) {
        case MacroIndicatorType::GDP: 
            function = "REAL_GDP"; 
            break;
        case MacroIndicatorType::Inflation: 
            function = "INFLATION"; 
            break;
        case MacroIndicatorType::Unemployment: 
            function = "UNEMPLOYMENT"; 
            break;
        case MacroIndicatorType::FedRate: 
            function = "FEDERAL_FUNDS_RATE"; 
            break;
        default: 
            co_return std::vector<EconomicIndicator>{};
    }

    auto json_opt = co_await PerformAVRequestAsync({{"function", function}});

    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::vector<EconomicIndicator>{};
    }

    std::vector<EconomicIndicator> results;
    try {
        const auto& data = (json_opt.value())["data"];
        
        if (!data.is_array()) {
            co_return std::vector<EconomicIndicator>{};
        }

        results.reserve(data.size());

        for (const auto& item : data) {
            EconomicIndicator ind;
            ind.date = TimeUtils::StringToDate(item.value("date", ""));
            ind.value = ParseDoubleSafe(item.value("value", "0"));

            results.push_back(ind);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Macro parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::map<std::string, double>> AlphaVantageProvider::GetTechnicalIndicator(
        const std::string& ticker, 
        const TechIndicatorType type, 
        const TimeFrame interval) {
    if (!HasCapability(ProviderCapability::TechIndicators)) {
        co_return std::map<std::string, double>{};
    }

    std::string func;
    std::string key; 
    
    switch (type) {
        case TechIndicatorType::RSI: 
            func = "RSI"; 
            key = "Technical Analysis: RSI"; 
            break;
        case TechIndicatorType::SMA: 
            func = "SMA"; 
            key = "Technical Analysis: SMA"; 
            break;
        case TechIndicatorType::EMA: 
            func = "EMA"; 
            key = "Technical Analysis: EMA"; 
            break;
        default: 
            co_return std::map<std::string, double>{}; 
    }

    std::string interval_str = ConvertInterval(interval);

    auto json_opt = co_await PerformAVRequestAsync({
                            {"function", func},
                            {"symbol", ticker},
                            {"interval", interval_str},
                            {"time_period", "14"},
                            {"series_type", "close"}
                        });

    if (!json_opt.has_value() || !json_opt->contains(key)) {
        co_return std::map<std::string, double>{};
    }

    try {
        const auto& series = (json_opt.value())[key];
        if (series.empty()) {
            co_return std::map<std::string, double>{};
        }
        
        const auto& val_obj = series.begin().value();
        
        std::map<std::string, double> result;
        for (auto it = val_obj.begin(); it != val_obj.end(); ++it) {
            result[it.key()] = ParseDoubleSafe(it.value().get<std::string>());
        }
        co_return result;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Tech Indicator parse error: {}", 
            ex.what());
        co_return std::map<std::string, double>{}; 
    }
}

boost::asio::awaitable<std::vector<TickerSearchResult>> AlphaVantageProvider::SearchStockTicker(
        const std::string& ticker) {
    auto json_opt = co_await PerformAVRequestAsync({{"function", "SYMBOL_SEARCH"}, {"keywords", ticker}});
    
    if (!json_opt.has_value() || !json_opt->contains("bestMatches")) {
        co_return std::vector<TickerSearchResult>{};
    }

    std::vector<TickerSearchResult> results;
    try {
        const auto& matches = (json_opt.value())["bestMatches"];
        
        if (!matches.is_array()) {
            co_return std::vector<TickerSearchResult>{};
        }

        results.reserve(matches.size());

        for (const auto& item : matches) {
            if (item.value("3. type", "") != "Equity") {
                continue; 
            }

            TickerSearchResult res;
            res.ticker = item.value("1. symbol", "");
            res.name = item.value("2. name", "");
            res.type = "Equity";
            res.region = item.value("4. region", "");

            results.push_back(res);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Search parse error: {}",
            ex.what());
    }
    co_return results;
}

bool AlphaVantageProvider::HasCapability(const CryptoCapability cap) const {
    return crypto_caps_.contains(cap);
}

boost::asio::awaitable<std::optional<double>> AlphaVantageProvider::GetCryptoPrice(
        const std::string& ticker) {
    if (!HasCapability(CryptoCapability::RealtimePrice)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformAVRequestAsync({
                            {"function", "CURRENCY_EXCHANGE_RATE"},
                            {"from_currency", ticker},
                            {"to_currency", "USD"}
                        });

    if (!json_opt.has_value() || !json_opt->contains("Realtime Currency Exchange Rate")) {
        co_return std::nullopt;
    }

    try {
        const auto& q = (json_opt.value())["Realtime Currency Exchange Rate"];

        std::string price_str = q.value("5. Exchange Rate", "");
        if (price_str.empty()) {
            co_return std::nullopt;
        }

        co_return std::stod(price_str);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Crypto Price parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> AlphaVantageProvider::GetCryptoHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (!HasCapability(CryptoCapability::History)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::string func = (interval == TimeFrame::Daily) ? "DIGITAL_CURRENCY_DAILY" : "DIGITAL_CURRENCY_WEEKLY";
    if (interval != TimeFrame::Daily && interval != TimeFrame::Weekly) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    auto json_opt = co_await PerformAVRequestAsync({
                            {"function", func},
                            {"symbol", ticker},
                            {"market", "USD"}
                        });

    std::string key = (interval == TimeFrame::Daily) ? 
                                    "Time Series (Digital Currency Daily)" : 
                                        "Time Series (Digital Currency Weekly)";
    
    if (!json_opt.has_value() || !json_opt->contains(key)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::vector<CryptoPriceCandle> results;
    try {
        const auto& series = (json_opt.value())[key];

        if (!series.is_array()) {
            co_return std::vector<CryptoPriceCandle>{};
        }

        results.reserve(series.size());

        for (auto it = series.begin(); it != series.end(); ++it) {
            Timestamp ts = TimeUtils::StringToTimestamp(it.key());
            if (ts < from || ts > to) {
                continue;
            }
            const auto& v = it.value();

            CryptoPriceCandle c;
            c.timestamp = ts;
            c.open = ParseDoubleSafe(v.value("1a. open (USD)", "0"));
            c.high = ParseDoubleSafe(v.value("2a. high (USD)", "0"));
            c.low = ParseDoubleSafe(v.value("3a. low (USD)", "0"));
            c.close = ParseDoubleSafe(v.value("4a. close (USD)", "0"));
            c.volume = ParseDoubleSafe(v.value("5. volume", "0"));

            results.push_back(c);
        }
        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
            return a.timestamp < b.timestamp;
        });
    } catch (const std::exception& ex) {
        Logger::Error(
            "[AlphaVantage] Crypto History parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<StockDividends>> AlphaVantageProvider::GetDividends(
        const std::string&, 
        const Date, 
        const Date) { 
    if (HasCapability(ProviderCapability::Dividends)) {
        Logger::Warn(
            "[AlphaVantage] Dividends not implemented.");
    }
    co_return std::vector<StockDividends>{}; 
}

boost::asio::awaitable<std::vector<StockSplit>> AlphaVantageProvider::GetStockSplits(
        const std::string&, 
        const Date, 
        const Date) { 
    if (HasCapability(ProviderCapability::StockSplits)) {
        Logger::Warn(
            "[AlphaVantage] Splits not implemented.");
    }
    co_return std::vector<StockSplit>{}; 
}

boost::asio::awaitable<std::vector<InsiderTransaction>> AlphaVantageProvider::GetInsiderTransactions(
        const std::string&, 
        const int) { 
    if (HasCapability(ProviderCapability::Insiders)) {
        Logger::Warn(
            "[AlphaVantage] Insiders not implemented.");
    }
    co_return std::vector<InsiderTransaction>{}; 
}

boost::asio::awaitable<std::vector<MarketNews>> AlphaVantageProvider::GetCompanyNews(
        const std::string&, 
        const int) { 
    if (HasCapability(ProviderCapability::News)) {
        Logger::Warn(
            "[AlphaVantage] News not implemented.");
    }
    co_return std::vector<MarketNews>{}; 
}

boost::asio::awaitable<std::vector<MarketNews>> AlphaVantageProvider::GetMarketNews(
        const std::string&, 
        const int) { 
    if (HasCapability(ProviderCapability::News)) {
        Logger::Warn(
            "[AlphaVantage] News not implemented.");
    }
    co_return std::vector<MarketNews>{}; 
}

boost::asio::awaitable<std::vector<CalendarEvent>> AlphaVantageProvider::GetEarningsCalendar(
        const Date, 
        const Date) { 
    if (HasCapability(ProviderCapability::Earnings)) {
        Logger::Warn(
            "[AlphaVantage] Earnings not implemented.");
    }
    co_return std::vector<CalendarEvent>{}; 
}

boost::asio::awaitable<std::optional<AnalystRating>> AlphaVantageProvider::GetAnalystRatings(
        const std::string&) { 
    if (HasCapability(ProviderCapability::AnalystRatings)) {
        Logger::Warn(
            "[AlphaVantage] Ratings not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::optional<CryptoAsset>> AlphaVantageProvider::GetCryptoAssetInfo(
        const std::string&) { 
    if (HasCapability(CryptoCapability::Metadata)) {
        Logger::Warn(
            "[AlphaVantage] Crypto Metadata not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::vector<CryptoAsset>> AlphaVantageProvider::GetCryptoTopList(int) { 
    if (HasCapability(CryptoCapability::TopList)) {
        Logger::Warn(
            "[AlphaVantage] Crypto TopList not implemented.");
    }
    co_return std::vector<CryptoAsset>{}; 
}

boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> AlphaVantageProvider::GetGlobalMetrics() { 
    if (HasCapability(CryptoCapability::GlobalMetrics)) {
        Logger::Warn(
            "[AlphaVantage] Crypto GlobalMetrics not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::optional<OrderBook>> AlphaVantageProvider::GetOrderBook(
        const std::string&, 
        const int) { 
    if (HasCapability(CryptoCapability::OrderBook)) {
        Logger::Warn(
            "[AlphaVantage] Crypto OrderBook not implemented.");
    }
    co_return std::nullopt; 
}

boost::asio::awaitable<std::vector<CryptoAsset>> AlphaVantageProvider::SearchAsset(const std::string&) {
    if (HasCapability(CryptoCapability::Metadata)) {
        Logger::Warn(
            "[AlphaVantage] Crypto search not optimized.");
    }
    co_return std::vector<CryptoAsset>{};
}

boost::asio::awaitable<std::optional<nlohmann::json>> AlphaVantageProvider::PerformAVRequestAsync(
        std::map<std::string, std::string> params) {
    auto json_opt = co_await PerformGetAsync("/query", params);
    
    if (json_opt.has_value()) {
        if (json_opt->contains("Error Message")) {
            Logger::Error(
                "[AlphaVantage] API Error: {}", 
                (json_opt.value())["Error Message"].get<std::string>());
            co_return std::nullopt;
        } else if (json_opt->contains("Note")) {
            Logger::Warn(
                "[AlphaVantage] Rate Limit Warning: {}", 
                (json_opt.value())["Note"].get<std::string>());
            co_return std::nullopt;
        } else if (json_opt->contains("Information")) {
            Logger::Warn(
                "[AlphaVantage] Info: {}", 
                (json_opt.value())["Information"].get<std::string>());
            co_return std::nullopt;
        }
    }
    co_return json_opt;
}

std::string AlphaVantageProvider::ConvertInterval(const TimeFrame tf) {
    switch (tf) {
        case TimeFrame::Minute1: 
            return "1min";
        case TimeFrame::Minute5: 
            return "5min";
        case TimeFrame::Minute15: 
            return "15min";
        case TimeFrame::Hourly: 
            return "60min";
        case TimeFrame::Daily:
            return "daily";
        default: 
            throw std::runtime_error("Interval not supported by AlphaVantage");
    }
}