#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <optional>
#include <algorithm>

#include "boost/asio/awaitable.hpp"

#include "../db/StockRepository.hpp"
#include "../db/CryptoRepository.hpp"
#include "ProviderManager.hpp"
#include "../common/TimeUtils.hpp"

class MarketService {
public:
    MarketService(
        std::shared_ptr<StockRepository> stock_repo,
        std::shared_ptr<CryptoRepository> crypto_repo);

    boost::asio::awaitable<std::optional<double>> GetStockPrice(const std::string& ticker);

    boost::asio::awaitable<std::optional<CompanyFullInfo>> GetCompanyProfile(const std::string& ticker);

    boost::asio::awaitable<std::vector<StockPriceCandle>> GetStockHistory(
        const std::string& ticker, 
        Timestamp from, 
        Timestamp to, 
        TimeFrame interval);

    boost::asio::awaitable<std::vector<StockDividends>> GetDividends(
        const std::string& ticker, 
        Date from, 
        Date to);

    boost::asio::awaitable<std::vector<CompanyFinancialReport>> GetFinancialReports(
        const std::string& ticker);

    boost::asio::awaitable<std::vector<TickerSearchResult>> SearchTicker(const std::string& query);

    boost::asio::awaitable<std::optional<double>> GetCryptoPrice(const std::string& ticker);

    boost::asio::awaitable<std::optional<CryptoAsset>> GetCryptoAssetInfo(const std::string& ticker);

    boost::asio::awaitable<std::vector<CryptoPriceCandle>> GetCryptoHistory(
        const std::string& ticker, 
        Timestamp from, 
        Timestamp to, 
        TimeFrame interval);

    boost::asio::awaitable<std::vector<CryptoAsset>> GetTopCryptoAssets(int limit = 50);

    boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> GetGlobalCryptoMetrics();

    boost::asio::awaitable<std::optional<OrderBook>> GetOrderBook(const std::string& ticker, int depth);

private:
    std::shared_ptr<StockRepository> stock_repo_;
    std::shared_ptr<CryptoRepository> crypto_repo_;
    ProviderManager& provider_manager_;

    boost::asio::awaitable<std::optional<int>> EnsureCompanyExists(const std::string& ticker);

    boost::asio::awaitable<std::optional<int>> EnsureCryptoAssetExists(const std::string& ticker);
};