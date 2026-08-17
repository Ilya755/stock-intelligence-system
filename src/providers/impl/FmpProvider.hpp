#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <exception>

#include "../IStockProvider.hpp"
#include "../BaseProvider.hpp"
#include "../ProviderCapabilities.hpp"
#include "../../common/TimeUtils.hpp"

class FmpProvider : public IStockProvider, public BaseProvider {
public:
    FmpProvider(std::shared_ptr<IHttpClient> client,
                    const std::string& name,
                    const std::string& api_key,
                    const std::string& base_url,
                    const Limits& limits,
                    const std::vector<std::string>& config_caps);

    std::string GetName() const override;

    bool HasCapability(const ProviderCapability cap) const override;

    boost::asio::awaitable<int> GetRemainingRequests() const override;

    boost::asio::awaitable<std::optional<double>> GetStockPrice(const std::string& ticker) override;

    boost::asio::awaitable<std::vector<StockPriceCandle>> GetStockHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) override;

    boost::asio::awaitable<std::optional<CompanyFullInfo>> GetCompanyProfile(
        const std::string& ticker) override;

    boost::asio::awaitable<std::vector<CompanyFinancialReport>> GetFinancialReports(
        const std::string& ticker) override;

    boost::asio::awaitable<std::vector<StockDividends>> GetDividends(
        const std::string& ticker, 
        const Date from, 
        const Date to) override;

    boost::asio::awaitable<std::vector<StockSplit>> GetStockSplits(
        const std::string& ticker, 
        const Date from, 
        const Date to) override;

    boost::asio::awaitable<std::vector<TickerSearchResult>> SearchStockTicker(
        const std::string& ticker) override;

    boost::asio::awaitable<std::vector<InsiderTransaction>> GetInsiderTransactions(
        const std::string&, 
        const int) override;

    boost::asio::awaitable<std::optional<AnalystRating>> GetAnalystRatings(const std::string&) override;

    boost::asio::awaitable<std::map<std::string, double>> GetTechnicalIndicator(
        const std::string&, 
        const TechIndicatorType, 
        const TimeFrame) override;

    boost::asio::awaitable<std::vector<MarketNews>> GetCompanyNews(
        const std::string&, 
        const int) override;

    boost::asio::awaitable<std::vector<MarketNews>> GetMarketNews(const std::string&, const int) override;

    boost::asio::awaitable<std::vector<CalendarEvent>> GetEarningsCalendar(const Date, const Date) override;

    boost::asio::awaitable<std::vector<EconomicIndicator>> GetMacroIndicator(
        const MacroIndicatorType) override;

private:
    std::string name_;
    std::unordered_set<ProviderCapability> stock_caps_;

    std::string ConvertInterval(const TimeFrame tf);
};