#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <chrono>

#include "../IStockProvider.hpp"
#include "../ICryptoProvider.hpp"
#include "../BaseProvider.hpp"
#include "../ProviderCapabilities.hpp"
#include "../CryptoCapabilities.hpp"
#include "../../common/TimeUtils.hpp"

class AlphaVantageProvider : public IStockProvider, public ICryptoProvider, public BaseProvider {
public:
    AlphaVantageProvider(std::shared_ptr<IHttpClient> client, const std::string& name,
                            const std::string& api_key, const std::string& base_url,
                            const Limits& limits, const std::vector<std::string>& config_caps);

    std::string GetName() const override;

    bool HasCapability(const ProviderCapability cap) const override;

    int GetRemainingRequests() const override;

    std::optional<double> GetStockPrice(const std::string& ticker) override;

    std::vector<StockPriceCandle> GetStockHistory(const std::string& ticker, 
                                                    const Timestamp from, const Timestamp to, 
                                                    const TimeFrame interval) override;

    std::optional<CompanyFullInfo> GetCompanyProfile(const std::string& ticker) override;

    std::vector<CompanyFinancialReport> GetFinancialReports(const std::string& ticker) override;

    std::vector<EconomicIndicator> GetMacroIndicator(const MacroIndicatorType type) override;

    std::map<std::string, double> GetTechnicalIndicator(const std::string& ticker, 
                                                            const TechIndicatorType type, 
                                                            const TimeFrame interval) override;

    std::vector<TickerSearchResult> SearchStockTicker(const std::string& ticker) override;

    bool HasCapability(const CryptoCapability cap) const override;

    std::optional<double> GetCryptoPrice(const std::string& ticker) override;

    std::vector<CryptoPriceCandle> GetCryptoHistory(const std::string& ticker, const Timestamp from, 
                                                        const Timestamp to, const TimeFrame interval) override;

    std::vector<StockDividends> GetDividends(const std::string&, const Date, const Date) override;

    std::vector<StockSplit> GetStockSplits(const std::string&, const Date, const Date) override;

    std::vector<InsiderTransaction> GetInsiderTransactions(const std::string&, const int) override;

    std::vector<MarketNews> GetCompanyNews(const std::string&, const int) override;

    std::vector<MarketNews> GetMarketNews(const std::string&, const int) override;

    std::vector<CalendarEvent> GetEarningsCalendar(const Date, const Date) override;

    std::optional<AnalystRating> GetAnalystRatings(const std::string&) override;

    std::optional<CryptoAsset> GetCryptoAssetInfo(const std::string&) override;

    std::vector<CryptoAsset> GetCryptoTopList(const int) override;

    std::optional<GlobalCryptoMetrics> GetGlobalMetrics() override;

    std::optional<OrderBook> GetOrderBook(const std::string&, const int) override;

    std::vector<CryptoAsset> SearchAsset(const std::string& query) override;

private:
    std::string name_;
    std::unordered_set<ProviderCapability> stock_caps_;
    std::unordered_set<CryptoCapability> crypto_caps_;

    std::optional<nlohmann::json> PerformAVRequest(std::map<std::string, std::string> params);

    std::string ConvertInterval(const TimeFrame tf);
};