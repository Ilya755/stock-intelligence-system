#pragma once

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <optional>
#include <sstream>

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/awaitable.hpp"

#include "../common/Config.hpp"
#include "../http/IHttpClient.hpp"
#include "../providers/IStockProvider.hpp"
#include "../providers/ICryptoProvider.hpp"

class ProviderManager {
public:
    static ProviderManager& GetInstance();

    void Init(const Config& config, boost::asio::any_io_executor executor);

    boost::asio::awaitable<std::optional<double>> GetStockPrice(const std::string& ticker);

    boost::asio::awaitable<std::vector<StockPriceCandle>> GetStockHistory(
        const std::string& ticker, 
        Timestamp from, 
        Timestamp to, 
        TimeFrame interval);

    boost::asio::awaitable<std::optional<CompanyFullInfo>> GetCompanyProfile(const std::string& ticker);

    boost::asio::awaitable<std::vector<TickerSearchResult>> SearchStockTicker(const std::string& query);

    boost::asio::awaitable<std::vector<CompanyFinancialReport>> GetFinancialReports(
        const std::string& ticker);

    boost::asio::awaitable<std::vector<StockDividends>> GetDividends(
        const std::string& ticker, 
        Date from, 
        Date to);

    boost::asio::awaitable<std::optional<double>> GetCryptoPrice(const std::string& ticker);

    boost::asio::awaitable<std::vector<CryptoPriceCandle>> GetCryptoHistory(
        const std::string& ticker, 
        Timestamp from, 
        Timestamp to, 
        TimeFrame interval);

    boost::asio::awaitable<std::optional<CryptoAsset>> GetCryptoAssetInfo(const std::string& ticker);

    boost::asio::awaitable<std::vector<CryptoAsset>> GetCryptoTopList(int limit = 50);

    boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> GetGlobalCryptoMetrics();

    boost::asio::awaitable<std::optional<OrderBook>> GetOrderBook(
        const std::string& ticker, 
        int depth = 10);

private:
    ProviderManager() = default;

    std::shared_ptr<IHttpClient> http_client_;

    struct StockProviderEntity {
        std::shared_ptr<IStockProvider> provider;
        int priority;
    };

    struct CryptoProviderEntity {
        std::shared_ptr<ICryptoProvider> provider;
        int priority;
    };

    std::vector<StockProviderEntity> stock_providers_;
    std::vector<CryptoProviderEntity> crypto_providers_;

    template <typename T>
    void AddProvider(std::shared_ptr<T> provider, int priority);

    template <typename ResultType, typename Func>
    boost::asio::awaitable<std::optional<ResultType>> ExecuteStockRequest(
        ProviderCapability cap, 
        Func action, 
        const std::string& action_name, 
        const std::string& context);

    template <typename ResultItem, typename Func>
    boost::asio::awaitable<std::vector<ResultItem>> ExecuteStockRequestList(
        ProviderCapability cap, 
        Func action, 
        const std::string& action_name, 
        const std::string& context);

    template <typename ResultType, typename Func>
    boost::asio::awaitable<std::optional<ResultType>> ExecuteCryptoRequest(
        CryptoCapability cap, 
        Func action, 
        const std::string& action_name, 
        const std::string& context);

    template <typename ResultItem, typename Func>
    boost::asio::awaitable<std::vector<ResultItem>> ExecuteCryptoRequestList(
        CryptoCapability cap, 
        Func action, 
        const std::string& action_name, 
        const std::string& context);
};