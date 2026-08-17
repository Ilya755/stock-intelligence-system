#include "FmpProvider.hpp"

#include "../../common/Logger.hpp"

FmpProvider::FmpProvider(std::shared_ptr<IHttpClient> client,
                            const std::string& name,
                            const std::string& api_key,
                            const std::string& base_url,
                            const Limits& limits,
                            const std::vector<std::string>& config_caps) 
    : BaseProvider(client, api_key, base_url, limits)
    , name_(name) {
        for (const auto& s : config_caps) {
            auto stock_cap = StringToProviderCapability(s);
            if (stock_cap.has_value()) {
                stock_caps_.insert(stock_cap.value());
            } else {
                Logger::Warn(
                    "[FMP] Unknown capability '{}' in config", 
                    s);
            }
        }
    }

std::string FmpProvider::GetName() const { 
    return name_; 
}

bool FmpProvider::HasCapability(const ProviderCapability cap) const {
    return stock_caps_.contains(cap);
}

boost::asio::awaitable<int> FmpProvider::GetRemainingRequests() const { co_return -1; } // ToDo

boost::asio::awaitable<std::optional<double>> FmpProvider::GetStockPrice(const std::string& ticker) {
    if (!HasCapability(ProviderCapability::PriceRealtime)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/quote/" + ticker);
    
    if (!json_opt.has_value() || json_opt->empty()) {
        co_return std::nullopt;
    }

    try {
        co_return (json_opt.value())[0]["price"].get<double>();
    } catch (const std::exception& ex) {
        Logger::Error(
            "[FMP] CurrentPrice parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<StockPriceCandle>> FmpProvider::GetStockHistory(const std::string& ticker, 
                                                            const Timestamp from, const Timestamp to, 
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


    std::string endpoint;
    
    if (interval == TimeFrame::Daily || interval == TimeFrame::Weekly || interval == TimeFrame::Monthly) {
        endpoint = "/historical-price-full/" + ticker;
    } else {
        std::string interval_str = ConvertInterval(interval);
        if (interval_str.empty()) {
            Logger::Warn(
                "[FMP] Interval {} not supported.", 
                ConvertInterval(interval));
            co_return std::vector<StockPriceCandle>{};
        }
        endpoint = "/historical-chart/" + interval_str + "/" + ticker;
    }

    std::map<std::string, std::string> params ={
        {"from", TimeUtils::DateToString(Date(std::chrono::floor<std::chrono::days>(from)))},
        {"to", TimeUtils::DateToString(Date(std::chrono::floor<std::chrono::days>(to)))}
    };

    auto json_opt = co_await PerformGetAsync(endpoint, params);

    if (!json_opt.has_value()) {
        co_return std::vector<StockPriceCandle>{};
    }

    std::vector<StockPriceCandle> results;
    try {
        const auto& j = json_opt.value();

        const auto& data = j.contains("historical") ? j["historical"] : j;
        
        if (!data.is_array()) {
            co_return std::vector<StockPriceCandle>{};
        }
        
        results.reserve(data.size());

        for (const auto& item : data) {
            StockPriceCandle c; 
            c.timestamp = TimeUtils::StringToTimestamp(item.value("date", ""));
            c.open = item.value("open", 0.0);
            c.high = item.value("high", 0.0);
            c.low = item.value("low", 0.0);
            c.close = item.value("close", 0.0);
            c.volume = item.value("volume", 0LL);
            
            results.push_back(c);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[FMP] History data processing error for {}: {}", 
            ticker, ex.what());
    }

    std::reverse(results.begin(), results.end()); 
    co_return results;
}

boost::asio::awaitable<std::optional<CompanyFullInfo>> FmpProvider::GetCompanyProfile(const std::string& ticker) {
    if (!HasCapability(ProviderCapability::CompanyProfile)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/profile/" + ticker);
    
    if (!json_opt.has_value() || json_opt->empty()){
        co_return std::nullopt;
    }

    try {
        const auto& j = (json_opt.value())[0];

        CompanyFullInfo info;
        info.ticker = j["symbol"];
        info.name = j["companyName"];
        info.currency = j["currency"];
        info.exchange = j["exchangeShortName"];
        info.sector = j.value("sector", "");
        info.industry = j.value("industry", "");
        info.description = j.value("description", "");
        info.country = j.value("country", "");
        info.updated_at = std::chrono::system_clock::now(); 

        co_return info;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[FMP] CompanyProfile Parse Error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt; 
    }
}

boost::asio::awaitable<std::vector<CompanyFinancialReport>> FmpProvider::GetFinancialReports(const std::string& ticker) {
    if (!HasCapability(ProviderCapability::FinancialsDeep)) {
        co_return std::vector<CompanyFinancialReport>{};
    }

    auto json_opt = co_await PerformGetAsync("/income-statement/" + ticker, {{"limit", "10"}});

    if (!json_opt.has_value()) {
        co_return std::vector<CompanyFinancialReport>{};
    }

    std::vector<CompanyFinancialReport> reports;
    try {
        const auto& data = json_opt.value();
        
        if (!data.is_array()) {
            co_return std::vector<CompanyFinancialReport>{};
        }
        
        reports.reserve(data.size());

        for (const auto& item : data) {
            CompanyFinancialReport r;
            r.period_date = TimeUtils::StringToDate(item.value("date", ""));
            r.currency = item.value("reportedCurrency", "USD");
            r.revenue = item.value("revenue", 0.0);
            r.net_income = item.value("netIncome", 0.0);
            r.total_debt = 0.0; 
            r.equity = 0.0;   

            std::string p = item.value("period", "FY");
            r.report_type = (p == "FY") ? ReportType::Annual : ReportType::Quarterly;

            reports.push_back(r);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[FMP] Financials error for {}: {}", 
            ticker, ex.what());
    }
    co_return reports;
}

boost::asio::awaitable<std::vector<StockDividends>> FmpProvider::GetDividends(const std::string& ticker, 
                                                        const Date from, const Date to) {
    if (!HasCapability(ProviderCapability::Dividends)) {
        co_return std::vector<StockDividends>{};
    }

    auto json_opt = co_await PerformGetAsync("/historical-price-full/stock_dividend/" + ticker);

    if (!json_opt.has_value() || !json_opt->contains("historical") || 
            !(json_opt.value())["historical"].is_array()) {
        co_return std::vector<StockDividends>{};
    }

    std::vector<StockDividends> result;
    try {
        const auto& historical = (json_opt.value())["historical"];

        if (!historical.is_array()) {
            co_return std::vector<StockDividends>{};
        }

        result.reserve(historical.size());
        
        for (const auto& item : historical) {
            StockDividends div;
            div.ex_date = TimeUtils::StringToDate(item["date"].get<std::string>());
            
            if (div.ex_date < from || div.ex_date > to) {
                continue; 
            }

            div.amount = item["adjDividend"].get<double>();
            div.payment_date = item.contains("paymentDate") && !item["paymentDate"].is_null() 
                ? std::optional<Date>(TimeUtils::StringToDate(item["paymentDate"].get<std::string>())) 
                : std::nullopt;

            result.push_back(div);
        }
    } catch (const std::exception& ex) { 
        Logger::Error(
            "[FMP] Failed to parse Dividends for {}: {}", 
            ticker, ex.what());
    }
    co_return result;
}

boost::asio::awaitable<std::vector<StockSplit>> FmpProvider::GetStockSplits(const std::string& ticker, 
                                                        const Date from, const Date to) {
    if (!HasCapability(ProviderCapability::StockSplits)) {
        co_return std::vector<StockSplit>{};
    }

    auto json_opt = co_await PerformGetAsync("/historical-price-full/stock_split/" + ticker);

    if (!json_opt.has_value() || !json_opt->contains("historical") || 
            !(json_opt.value())["historical"].is_array()) {
        co_return std::vector<StockSplit>{};
    }

    const auto& historical = (json_opt.value())["historical"];

    std::vector<StockSplit> result;
    
    result.reserve(historical.size());
    
    for (const auto& item : historical) {
        try {
            StockSplit split;

            split.date = TimeUtils::StringToDate(item["date"].get<std::string>());
            if (split.date < from || split.date > to) {
                continue;
            }

            split.numerator = item["numerator"].get<double>();
            split.denominator = item["denominator"].get<double>();
            result.push_back(split);
        } catch (const std::exception& ex) {
            Logger::Warn(
                "[FMP] Failed to parse Stock Split item for {}: {}. JSON segment: {}", 
                ticker, ex.what(), item.dump());
            continue;
        }
    }
    co_return result;
}

boost::asio::awaitable<std::vector<TickerSearchResult>> FmpProvider::SearchStockTicker(const std::string& ticker) {
    auto json_opt = co_await PerformGetAsync("/search", {{"query", ticker}, {"limit", "10"}});

    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<TickerSearchResult>{};
    }

    std::vector<TickerSearchResult> results;
    try {
        const auto& data = json_opt.value();
    
        results.reserve(data.size());

        for (const auto& item : data) {
            TickerSearchResult res;
            res.name = item.value("name", "");
            res.ticker = item.value("symbol", "");
            res.type = "Equity";
            res.region = item.value("currency", "USD");
            
            results.push_back(res);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[FMP] Search parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<InsiderTransaction>> FmpProvider::GetInsiderTransactions(const std::string&, const int) {
    if (HasCapability(ProviderCapability::Insiders)) {
        Logger::Warn(
            "[FMP] Insiders not implemented.");
    }
    co_return std::vector<InsiderTransaction>{};
}

boost::asio::awaitable<std::optional<AnalystRating>> FmpProvider::GetAnalystRatings(const std::string&) {
    if (HasCapability(ProviderCapability::AnalystRatings)) {
        Logger::Warn(
            "[FMP] Ratings not implemented.");
    }
    co_return std::nullopt;
}

boost::asio::awaitable<std::map<std::string, double>> FmpProvider::GetTechnicalIndicator(const std::string&, const TechIndicatorType, 
                                                                    const TimeFrame) {
    if (HasCapability(ProviderCapability::TechIndicators)) {
        Logger::Warn(
            "[FMP] TechIndicators not implemented.");
    }
    co_return std::map<std::string, double>{};
}

boost::asio::awaitable<std::vector<MarketNews>> FmpProvider::GetCompanyNews(const std::string&, const int) {
    if (HasCapability(ProviderCapability::News)) {
        Logger::Warn(
            "[FMP] News not implemented.");
    }
    co_return std::vector<MarketNews>{};
}

boost::asio::awaitable<std::vector<MarketNews>> FmpProvider::GetMarketNews(const std::string&, const int) {
    if (HasCapability(ProviderCapability::News)) {
        Logger::Warn(
            "[FMP] News not implemented.");
    }
    co_return std::vector<MarketNews>{};
}

boost::asio::awaitable<std::vector<CalendarEvent>> FmpProvider::GetEarningsCalendar(const Date, const Date) {
    if (HasCapability(ProviderCapability::Earnings)) {
        Logger::Warn(
            "[FMP] Earnings not implemented.");
    }
    co_return std::vector<CalendarEvent>{};
}

boost::asio::awaitable<std::vector<EconomicIndicator>> FmpProvider::GetMacroIndicator(const MacroIndicatorType) {
    if (HasCapability(ProviderCapability::MacroEconomics)) {
        Logger::Warn(
            "[FMP] Macro not implemented.");
    }
    co_return std::vector<EconomicIndicator>{};
}

std::string FmpProvider::ConvertInterval(const TimeFrame tf) {
    switch (tf) {
        case TimeFrame::Minute1: 
            return "1min";
        case TimeFrame::Minute5: 
            return "5min";
        case TimeFrame::Minute15: 
            return "15min";
        case TimeFrame::Hourly: 
            return "1hour";
        default: 
            throw std::runtime_error("Interval not supported by FMP");
    }
}