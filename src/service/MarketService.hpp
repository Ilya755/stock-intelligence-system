#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <optional>
#include <algorithm>

#include "../db/StockRepository.hpp"
#include "../db/CryptoRepository.hpp"
#include "ProviderManager.hpp"
#include "../common/TimeUtils.hpp"

class MarketService {
public:
    MarketService(std::shared_ptr<StockRepository> stock_repo,
                  std::shared_ptr<CryptoRepository> crypto_repo);

    std::optional<double> GetStockPrice(const std::string& ticker);

    std::optional<CompanyFullInfo> GetCompanyProfile(const std::string& ticker);

    std::vector<StockPriceCandle> GetStockHistory(const std::string& ticker, 
                                                    Timestamp from, 
                                                    Timestamp to, 
                                                    TimeFrame interval);

    std::vector<StockDividends> GetDividends(const std::string& ticker, Date from, Date to);

    std::vector<CompanyFinancialReport> GetFinancialReports(const std::string& ticker);

    std::vector<TickerSearchResult> SearchTicker(const std::string& query);

    std::optional<double> GetCryptoPrice(const std::string& ticker);

    std::optional<CryptoAsset> GetCryptoAssetInfo(const std::string& ticker);

    std::vector<CryptoPriceCandle> GetCryptoHistory(const std::string& ticker, 
                                                        Timestamp from, 
                                                        Timestamp to, 
                                                        TimeFrame interval);

    std::vector<CryptoAsset> GetTopCryptoAssets(int limit = 50);

private:
    std::shared_ptr<StockRepository> stock_repo_;
    std::shared_ptr<CryptoRepository> crypto_repo_;
    ProviderManager& provider_manager_;

    std::optional<int> EnsureCompanyExists(const std::string& ticker);

    std::optional<int> EnsureCryptoAssetExists(const std::string& ticker);
};