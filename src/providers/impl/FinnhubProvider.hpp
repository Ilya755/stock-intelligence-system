#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <algorithm>
#include <map>
#include <chrono>

#include "../IStockProvider.hpp"
#include "../ICryptoProvider.hpp"
#include "../BaseProvider.hpp"
#include "../ProviderCapabilities.hpp"
#include "../CryptoCapabilities.hpp"
#include "../../common/TimeUtils.hpp"

class FinnhubProvider : public IStockProvider, public ICryptoProvider, public BaseProvider {
public:
    FinnhubProvider(std::shared_ptr<IHttpClient> client,
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

    boost::asio::awaitable<std::vector<TickerSearchResult>> SearchStockTicker(
        const std::string& ticker) override;

    boost::asio::awaitable<std::vector<StockDividends>> GetDividends(
        const std::string& ticker, 
        const Date from, 
        const Date to) override;

    boost::asio::awaitable<std::optional<AnalystRating>> GetAnalystRatings(
        const std::string& ticker) override;

    boost::asio::awaitable<std::vector<MarketNews>> GetCompanyNews(
        const std::string& ticker, 
        const int limit) override;

    boost::asio::awaitable<std::vector<MarketNews>> GetMarketNews(
        const std::string& category, 
        const int limit) override;

    bool HasCapability(const CryptoCapability cap) const override;

    boost::asio::awaitable<std::vector<CryptoPriceCandle>> GetCryptoHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) override;

    boost::asio::awaitable<std::vector<StockSplit>> GetStockSplits(
        const std::string&, 
        const Date, 
        const Date) override;

    boost::asio::awaitable<std::vector<CompanyFinancialReport>> GetFinancialReports(
        const std::string&) override;

    boost::asio::awaitable<std::vector<InsiderTransaction>> GetInsiderTransactions(
        const std::string&,
        const int) override;

    boost::asio::awaitable<std::map<std::string, double>> GetTechnicalIndicator(
        const std::string&, 
        const TechIndicatorType, 
        const TimeFrame) override;

    boost::asio::awaitable<std::vector<CalendarEvent>> GetEarningsCalendar(const Date, const Date) override;

    boost::asio::awaitable<std::vector<EconomicIndicator>> GetMacroIndicator(
        const MacroIndicatorType) override;

    boost::asio::awaitable<std::optional<double>> GetCryptoPrice(const std::string&) override;

    boost::asio::awaitable<std::optional<CryptoAsset>> GetCryptoAssetInfo(const std::string&) override;

    boost::asio::awaitable<std::vector<CryptoAsset>> GetCryptoTopList(const int) override;

    boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> GetGlobalMetrics() override;

    boost::asio::awaitable<std::optional<OrderBook>> GetOrderBook(const std::string&, const int) override;
    
    boost::asio::awaitable<std::vector<CryptoAsset>> SearchAsset(const std::string&) override;

private:
    std::string name_;
    std::unordered_set<ProviderCapability> stock_caps_;
    std::unordered_set<CryptoCapability> crypto_caps_;

    std::string ConvertInterval(const TimeFrame tf);
};